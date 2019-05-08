// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "common/microprofile.h"
#include "video_core/memory_manager.h"
#include "video_core/texture_cache/surface_base.h"
#include "video_core/texture_cache/surface_params.h"
#include "video_core/textures/convert.h"

namespace VideoCommon {

MICROPROFILE_DEFINE(GPU_Load_Texture, "GPU", "Texture Load", MP_RGB(128, 192, 128));
MICROPROFILE_DEFINE(GPU_Flush_Texture, "GPU", "Texture Flush", MP_RGB(128, 192, 128));

using Tegra::Texture::ConvertFromGuestToHost;
using VideoCore::MortonSwizzleMode;

SurfaceBaseImpl::SurfaceBaseImpl(GPUVAddr gpu_addr, const SurfaceParams& params)
    : params{params}, gpu_addr{gpu_addr}, layer_size{params.GetGuestLayerSize()},
      guest_memory_size{params.GetGuestSizeInBytes()}, host_memory_size{
                                                           params.GetHostSizeInBytes()} {
    mipmap_offsets.reserve(params.num_levels);
    mipmap_sizes.reserve(params.num_levels);

    std::size_t offset = 0;
    for (u32 level = 0; level < params.num_levels; ++level) {
        const std::size_t mipmap_size{params.GetGuestMipmapSize(level)};
        mipmap_sizes.push_back(mipmap_size);
        mipmap_offsets.push_back(offset);
        offset += mipmap_size;
    }
}

void SurfaceBaseImpl::SwizzleFunc(MortonSwizzleMode mode, u8* memory, const SurfaceParams& params,
                                  u8* buffer, u32 level) {
    const u32 width{params.GetMipWidth(level)};
    const u32 height{params.GetMipHeight(level)};
    const u32 block_height{params.GetMipBlockHeight(level)};
    const u32 block_depth{params.GetMipBlockDepth(level)};

    std::size_t guest_offset{mipmap_offsets[level]};
    if (params.is_layered) {
        std::size_t host_offset{0};
        const std::size_t guest_stride = layer_size;
        const std::size_t host_stride = params.GetHostLayerSize(level);
        for (u32 layer = 0; layer < params.depth; ++layer) {
            MortonSwizzle(mode, params.pixel_format, width, block_height, height, block_depth, 1,
                          params.tile_width_spacing, buffer + host_offset, memory + guest_offset);
            guest_offset += guest_stride;
            host_offset += host_stride;
        }
    } else {
        MortonSwizzle(mode, params.pixel_format, width, block_height, height, block_depth,
                      params.GetMipDepth(level), params.tile_width_spacing, buffer,
                      memory + guest_offset);
    }
}

void SurfaceBaseImpl::LoadBuffer(Tegra::MemoryManager& memory_manager,
                                 std::vector<u8>& staging_buffer) {
    MICROPROFILE_SCOPE(GPU_Load_Texture);
    const auto host_ptr{memory_manager.GetPointer(gpu_addr)};
    if (!host_ptr) {
        return;
    }
    if (params.is_tiled) {
        ASSERT_MSG(params.block_width == 1, "Block width is defined as {} on texture target {}",
                   params.block_width, static_cast<u32>(params.target));
        for (u32 level = 0; level < params.num_levels; ++level) {
            const std::size_t host_offset{params.GetHostMipmapLevelOffset(level)};
            SwizzleFunc(MortonSwizzleMode::MortonToLinear, host_ptr, params,
                        staging_buffer.data() + host_offset, level);
        }
    } else {
        ASSERT_MSG(params.num_levels == 1, "Linear mipmap loading is not implemented");
        const u32 bpp{params.GetBytesPerPixel()};
        const u32 block_width{params.GetDefaultBlockWidth()};
        const u32 block_height{params.GetDefaultBlockHeight()};
        const u32 width{(params.width + block_width - 1) / block_width};
        const u32 height{(params.height + block_height - 1) / block_height};
        const u32 copy_size{width * bpp};
        if (params.pitch == copy_size) {
            std::memcpy(staging_buffer.data(), host_ptr, params.GetHostSizeInBytes());
        } else {
            const u8* start{host_ptr};
            u8* write_to{staging_buffer.data()};
            for (u32 h = height; h > 0; --h) {
                std::memcpy(write_to, start, copy_size);
                start += params.pitch;
                write_to += copy_size;
            }
        }
    }

    for (u32 level = 0; level < params.num_levels; ++level) {
        const std::size_t host_offset{params.GetHostMipmapLevelOffset(level)};
        ConvertFromGuestToHost(staging_buffer.data() + host_offset, params.pixel_format,
                               params.GetMipWidth(level), params.GetMipHeight(level),
                               params.GetMipDepth(level), true, true);
    }
}

void SurfaceBaseImpl::FlushBuffer(Tegra::MemoryManager& memory_manager,
                                  std::vector<u8>& staging_buffer) {
    MICROPROFILE_SCOPE(GPU_Flush_Texture);
    const auto host_ptr{memory_manager.GetPointer(gpu_addr)};
    if (!host_ptr) {
        return;
    }
    if (params.is_tiled) {
        ASSERT_MSG(params.block_width == 1, "Block width is defined as {}", params.block_width);
        for (u32 level = 0; level < params.num_levels; ++level) {
            const std::size_t host_offset{params.GetHostMipmapLevelOffset(level)};
            SwizzleFunc(MortonSwizzleMode::LinearToMorton, host_ptr, params,
                        staging_buffer.data() + host_offset, level);
        }
    } else {
        ASSERT(params.target == SurfaceTarget::Texture2D);
        ASSERT(params.num_levels == 1);

        const u32 bpp{params.GetBytesPerPixel()};
        const u32 copy_size{params.width * bpp};
        if (params.pitch == copy_size) {
            std::memcpy(host_ptr, staging_buffer.data(), guest_memory_size);
        } else {
            u8* start{host_ptr};
            const u8* read_to{staging_buffer.data()};
            for (u32 h = params.height; h > 0; --h) {
                std::memcpy(start, read_to, copy_size);
                start += params.pitch;
                read_to += copy_size;
            }
        }
    }
}

} // namespace VideoCommon
