// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/morton.h"
#include "video_core/texture_cache/surface_base.h"
#include "video_core/texture_cache/surface_params.h"
#include "video_core/textures/convert.h"

namespace VideoCommon {

using Tegra::Texture::ConvertFromGuestToHost;
using VideoCore::MortonSwizzleMode;

namespace {
void SwizzleFunc(MortonSwizzleMode mode, u8* memory, const SurfaceParams& params, u8* buffer,
                 u32 level) {
    const u32 width{params.GetMipWidth(level)};
    const u32 height{params.GetMipHeight(level)};
    const u32 block_height{params.GetMipBlockHeight(level)};
    const u32 block_depth{params.GetMipBlockDepth(level)};

    std::size_t guest_offset{params.GetGuestMipmapLevelOffset(level)};
    if (params.IsLayered()) {
        std::size_t host_offset{0};
        const std::size_t guest_stride = params.GetGuestLayerSize();
        const std::size_t host_stride = params.GetHostLayerSize(level);
        for (u32 layer = 0; layer < params.GetNumLayers(); layer++) {
            MortonSwizzle(mode, params.GetPixelFormat(), width, block_height, height, block_depth,
                          1, params.GetTileWidthSpacing(), buffer + host_offset,
                          memory + guest_offset);
            guest_offset += guest_stride;
            host_offset += host_stride;
        }
    } else {
        MortonSwizzle(mode, params.GetPixelFormat(), width, block_height, height, block_depth,
                      params.GetMipDepth(level), params.GetTileWidthSpacing(), buffer,
                      memory + guest_offset);
    }
}
} // Anonymous namespace

SurfaceBaseImpl::SurfaceBaseImpl(const SurfaceParams& params) : params{params} {
    staging_buffer.resize(params.GetHostSizeInBytes());
}

SurfaceBaseImpl::~SurfaceBaseImpl() = default;

void SurfaceBaseImpl::LoadBuffer() {
    if (params.IsTiled()) {
        ASSERT_MSG(params.GetBlockWidth() == 1, "Block width is defined as {} on texture target {}",
                   params.GetBlockWidth(), static_cast<u32>(params.GetTarget()));
        for (u32 level = 0; level < params.GetNumLevels(); ++level) {
            u8* const buffer{GetStagingBufferLevelData(level)};
            SwizzleFunc(MortonSwizzleMode::MortonToLinear, host_ptr, params, buffer, level);
        }
    } else {
        ASSERT_MSG(params.GetNumLevels() == 1, "Linear mipmap loading is not implemented");
        const u32 bpp{GetFormatBpp(params.GetPixelFormat()) / CHAR_BIT};
        const u32 block_width{params.GetDefaultBlockWidth()};
        const u32 block_height{params.GetDefaultBlockHeight()};
        const u32 width{(params.GetWidth() + block_width - 1) / block_width};
        const u32 height{(params.GetHeight() + block_height - 1) / block_height};
        const u32 copy_size{width * bpp};
        if (params.GetPitch() == copy_size) {
            std::memcpy(staging_buffer.data(), host_ptr, params.GetHostSizeInBytes());
        } else {
            const u8* start{host_ptr};
            u8* write_to{staging_buffer.data()};
            for (u32 h = height; h > 0; --h) {
                std::memcpy(write_to, start, copy_size);
                start += params.GetPitch();
                write_to += copy_size;
            }
        }
    }

    for (u32 level = 0; level < params.GetNumLevels(); ++level) {
        ConvertFromGuestToHost(GetStagingBufferLevelData(level), params.GetPixelFormat(),
                               params.GetMipWidth(level), params.GetMipHeight(level),
                               params.GetMipDepth(level), true, true);
    }
}

void SurfaceBaseImpl::FlushBuffer() {
    if (params.IsTiled()) {
        ASSERT_MSG(params.GetBlockWidth() == 1, "Block width is defined as {}",
                   params.GetBlockWidth());
        for (u32 level = 0; level < params.GetNumLevels(); ++level) {
            u8* const buffer = GetStagingBufferLevelData(level);
            SwizzleFunc(MortonSwizzleMode::LinearToMorton, GetHostPtr(), params, buffer, level);
        }
    } else {
        UNIMPLEMENTED();
        /*
        ASSERT(params.GetTarget() == SurfaceTarget::Texture2D);
        ASSERT(params.GetNumLevels() == 1);

        const u32 bpp{params.GetFormatBpp() / 8};
        const u32 copy_size{params.GetWidth() * bpp};
        if (params.GetPitch() == copy_size) {
            std::memcpy(host_ptr, staging_buffer.data(), GetSizeInBytes());
        } else {
            u8* start{host_ptr};
            const u8* read_to{staging_buffer.data()};
            for (u32 h = params.GetHeight(); h > 0; --h) {
                std::memcpy(start, read_to, copy_size);
                start += params.GetPitch();
                read_to += copy_size;
            }
        }
        */
    }
}

} // namespace VideoCommon
