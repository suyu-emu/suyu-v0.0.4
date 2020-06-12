// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <string>
#include <tuple>

#include "common/alignment.h"
#include "common/bit_util.h"
#include "core/core.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/format_lookup_table.h"
#include "video_core/texture_cache/surface_params.h"

namespace VideoCommon {

using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::PixelFormatFromDepthFormat;
using VideoCore::Surface::PixelFormatFromRenderTargetFormat;
using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceTargetFromTextureType;
using VideoCore::Surface::SurfaceType;

namespace {

SurfaceTarget TextureTypeToSurfaceTarget(Tegra::Shader::TextureType type, bool is_array) {
    switch (type) {
    case Tegra::Shader::TextureType::Texture1D:
        return is_array ? SurfaceTarget::Texture1DArray : SurfaceTarget::Texture1D;
    case Tegra::Shader::TextureType::Texture2D:
        return is_array ? SurfaceTarget::Texture2DArray : SurfaceTarget::Texture2D;
    case Tegra::Shader::TextureType::Texture3D:
        ASSERT(!is_array);
        return SurfaceTarget::Texture3D;
    case Tegra::Shader::TextureType::TextureCube:
        return is_array ? SurfaceTarget::TextureCubeArray : SurfaceTarget::TextureCubemap;
    default:
        UNREACHABLE();
        return SurfaceTarget::Texture2D;
    }
}

SurfaceTarget ImageTypeToSurfaceTarget(Tegra::Shader::ImageType type) {
    switch (type) {
    case Tegra::Shader::ImageType::Texture1D:
        return SurfaceTarget::Texture1D;
    case Tegra::Shader::ImageType::TextureBuffer:
        return SurfaceTarget::TextureBuffer;
    case Tegra::Shader::ImageType::Texture1DArray:
        return SurfaceTarget::Texture1DArray;
    case Tegra::Shader::ImageType::Texture2D:
        return SurfaceTarget::Texture2D;
    case Tegra::Shader::ImageType::Texture2DArray:
        return SurfaceTarget::Texture2DArray;
    case Tegra::Shader::ImageType::Texture3D:
        return SurfaceTarget::Texture3D;
    default:
        UNREACHABLE();
        return SurfaceTarget::Texture2D;
    }
}

constexpr u32 GetMipmapSize(bool uncompressed, u32 mip_size, u32 tile) {
    return uncompressed ? mip_size : std::max(1U, (mip_size + tile - 1) / tile);
}

} // Anonymous namespace

SurfaceParams SurfaceParams::CreateForTexture(const FormatLookupTable& lookup_table,
                                              const Tegra::Texture::TICEntry& tic,
                                              const VideoCommon::Shader::Sampler& entry) {
    SurfaceParams params;
    params.is_tiled = tic.IsTiled();
    params.srgb_conversion = tic.IsSrgbConversionEnabled();
    params.block_width = params.is_tiled ? tic.BlockWidth() : 0;
    params.block_height = params.is_tiled ? tic.BlockHeight() : 0;
    params.block_depth = params.is_tiled ? tic.BlockDepth() : 0;
    params.tile_width_spacing = params.is_tiled ? (1 << tic.tile_width_spacing.Value()) : 1;
    params.pixel_format = lookup_table.GetPixelFormat(
        tic.format, params.srgb_conversion, tic.r_type, tic.g_type, tic.b_type, tic.a_type);
    params.type = GetFormatType(params.pixel_format);
    if (entry.is_shadow && params.type == SurfaceType::ColorTexture) {
        switch (params.pixel_format) {
        case PixelFormat::R16_UNORM:
        case PixelFormat::R16_FLOAT:
            params.pixel_format = PixelFormat::D16_UNORM;
            break;
        case PixelFormat::R32_FLOAT:
            params.pixel_format = PixelFormat::D32_FLOAT;
            break;
        default:
            UNIMPLEMENTED_MSG("Unimplemented shadow convert format: {}",
                              static_cast<u32>(params.pixel_format));
        }
        params.type = GetFormatType(params.pixel_format);
    }
    // TODO: on 1DBuffer we should use the tic info.
    if (tic.IsBuffer()) {
        params.target = SurfaceTarget::TextureBuffer;
        params.width = tic.Width();
        params.pitch = params.width * params.GetBytesPerPixel();
        params.height = 1;
        params.depth = 1;
        params.num_levels = 1;
        params.emulated_levels = 1;
        params.is_layered = false;
    } else {
        params.target = TextureTypeToSurfaceTarget(entry.type, entry.is_array);
        params.width = tic.Width();
        params.height = tic.Height();
        params.depth = tic.Depth();
        params.pitch = params.is_tiled ? 0 : tic.Pitch();
        if (params.target == SurfaceTarget::TextureCubemap ||
            params.target == SurfaceTarget::TextureCubeArray) {
            params.depth *= 6;
        }
        params.num_levels = tic.max_mip_level + 1;
        params.emulated_levels = std::min(params.num_levels, params.MaxPossibleMipmap());
        params.is_layered = params.IsLayered();
    }
    return params;
}

SurfaceParams SurfaceParams::CreateForImage(const FormatLookupTable& lookup_table,
                                            const Tegra::Texture::TICEntry& tic,
                                            const VideoCommon::Shader::Image& entry) {
    SurfaceParams params;
    params.is_tiled = tic.IsTiled();
    params.srgb_conversion = tic.IsSrgbConversionEnabled();
    params.block_width = params.is_tiled ? tic.BlockWidth() : 0;
    params.block_height = params.is_tiled ? tic.BlockHeight() : 0;
    params.block_depth = params.is_tiled ? tic.BlockDepth() : 0;
    params.tile_width_spacing = params.is_tiled ? (1 << tic.tile_width_spacing.Value()) : 1;
    params.pixel_format = lookup_table.GetPixelFormat(
        tic.format, params.srgb_conversion, tic.r_type, tic.g_type, tic.b_type, tic.a_type);
    params.type = GetFormatType(params.pixel_format);
    params.target = ImageTypeToSurfaceTarget(entry.type);
    // TODO: on 1DBuffer we should use the tic info.
    if (tic.IsBuffer()) {
        params.target = SurfaceTarget::TextureBuffer;
        params.width = tic.Width();
        params.pitch = params.width * params.GetBytesPerPixel();
        params.height = 1;
        params.depth = 1;
        params.num_levels = 1;
        params.emulated_levels = 1;
        params.is_layered = false;
    } else {
        params.width = tic.Width();
        params.height = tic.Height();
        params.depth = tic.Depth();
        params.pitch = params.is_tiled ? 0 : tic.Pitch();
        if (params.target == SurfaceTarget::TextureCubemap ||
            params.target == SurfaceTarget::TextureCubeArray) {
            params.depth *= 6;
        }
        params.num_levels = tic.max_mip_level + 1;
        params.emulated_levels = std::min(params.num_levels, params.MaxPossibleMipmap());
        params.is_layered = params.IsLayered();
    }
    return params;
}

SurfaceParams SurfaceParams::CreateForDepthBuffer(Tegra::Engines::Maxwell3D& maxwell3d) {
    const auto& regs = maxwell3d.regs;
    const auto block_depth = std::min(regs.zeta.memory_layout.block_depth.Value(), 5U);
    const bool is_layered = regs.zeta_layers > 1 && block_depth == 0;
    const auto pixel_format = PixelFormatFromDepthFormat(regs.zeta.format);
    return {
        .is_tiled = regs.zeta.memory_layout.type ==
                    Tegra::Engines::Maxwell3D::Regs::InvMemoryLayout::BlockLinear,
        .srgb_conversion = false,
        .is_layered = is_layered,
        .block_width = std::min(regs.zeta.memory_layout.block_width.Value(), 5U),
        .block_height = std::min(regs.zeta.memory_layout.block_height.Value(), 5U),
        .block_depth = block_depth,
        .tile_width_spacing = 1,
        .width = regs.zeta_width,
        .height = regs.zeta_height,
        .depth = is_layered ? regs.zeta_layers.Value() : 1U,
        .pitch = 0,
        .num_levels = 1,
        .emulated_levels = 1,
        .pixel_format = pixel_format,
        .type = GetFormatType(pixel_format),
        .target = is_layered ? SurfaceTarget::Texture2DArray : SurfaceTarget::Texture2D,
    };
}

SurfaceParams SurfaceParams::CreateForFramebuffer(Tegra::Engines::Maxwell3D& maxwell3d,
                                                  std::size_t index) {
    const auto& config{maxwell3d.regs.rt[index]};
    SurfaceParams params;
    params.is_tiled =
        config.memory_layout.type == Tegra::Engines::Maxwell3D::Regs::InvMemoryLayout::BlockLinear;
    params.srgb_conversion = config.format == Tegra::RenderTargetFormat::B8G8R8A8_SRGB ||
                             config.format == Tegra::RenderTargetFormat::A8B8G8R8_SRGB;
    params.block_width = config.memory_layout.block_width;
    params.block_height = config.memory_layout.block_height;
    params.block_depth = config.memory_layout.block_depth;
    params.tile_width_spacing = 1;
    params.pixel_format = PixelFormatFromRenderTargetFormat(config.format);
    params.type = GetFormatType(params.pixel_format);
    if (params.is_tiled) {
        params.pitch = 0;
        params.width = config.width;
    } else {
        const u32 bpp = GetFormatBpp(params.pixel_format) / CHAR_BIT;
        params.pitch = config.width;
        params.width = params.pitch / bpp;
    }
    params.height = config.height;
    params.num_levels = 1;
    params.emulated_levels = 1;

    if (config.memory_layout.is_3d != 0) {
        params.depth = config.layers.Value();
        params.is_layered = false;
        params.target = SurfaceTarget::Texture3D;
    } else if (config.layers > 1) {
        params.depth = config.layers.Value();
        params.is_layered = true;
        params.target = SurfaceTarget::Texture2DArray;
    } else {
        params.depth = 1;
        params.is_layered = false;
        params.target = SurfaceTarget::Texture2D;
    }
    return params;
}

SurfaceParams SurfaceParams::CreateForFermiCopySurface(
    const Tegra::Engines::Fermi2D::Regs::Surface& config) {
    const bool is_tiled = !config.linear;
    const auto pixel_format = PixelFormatFromRenderTargetFormat(config.format);

    SurfaceParams params{
        .is_tiled = is_tiled,
        .srgb_conversion = config.format == Tegra::RenderTargetFormat::B8G8R8A8_SRGB ||
                           config.format == Tegra::RenderTargetFormat::A8B8G8R8_SRGB,
        .block_width = is_tiled ? std::min(config.BlockWidth(), 5U) : 0U,
        .block_height = is_tiled ? std::min(config.BlockHeight(), 5U) : 0U,
        .block_depth = is_tiled ? std::min(config.BlockDepth(), 5U) : 0U,
        .tile_width_spacing = 1,
        .width = config.width,
        .height = config.height,
        .depth = 1,
        .pitch = config.pitch,
        .num_levels = 1,
        .emulated_levels = 1,
        .pixel_format = pixel_format,
        .type = GetFormatType(pixel_format),
        // TODO(Rodrigo): Try to guess texture arrays from parameters
        .target = SurfaceTarget::Texture2D,
    };

    params.is_layered = params.IsLayered();
    return params;
}

VideoCore::Surface::SurfaceTarget SurfaceParams::ExpectedTarget(
    const VideoCommon::Shader::Sampler& entry) {
    return TextureTypeToSurfaceTarget(entry.type, entry.is_array);
}

VideoCore::Surface::SurfaceTarget SurfaceParams::ExpectedTarget(
    const VideoCommon::Shader::Image& entry) {
    return ImageTypeToSurfaceTarget(entry.type);
}

bool SurfaceParams::IsLayered() const {
    switch (target) {
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        return true;
    default:
        return false;
    }
}

// Auto block resizing algorithm from:
// https://cgit.freedesktop.org/mesa/mesa/tree/src/gallium/drivers/nouveau/nv50/nv50_miptree.c
u32 SurfaceParams::GetMipBlockHeight(u32 level) const {
    if (level == 0) {
        return this->block_height;
    }

    const u32 height_new{GetMipHeight(level)};
    const u32 default_block_height{GetDefaultBlockHeight()};
    const u32 blocks_in_y{(height_new + default_block_height - 1) / default_block_height};
    const u32 block_height_new = Common::Log2Ceil32(blocks_in_y);
    return std::clamp(block_height_new, 3U, 7U) - 3U;
}

u32 SurfaceParams::GetMipBlockDepth(u32 level) const {
    if (level == 0) {
        return this->block_depth;
    }
    if (is_layered) {
        return 0;
    }

    const u32 depth_new{GetMipDepth(level)};
    const u32 block_depth_new = Common::Log2Ceil32(depth_new);
    if (block_depth_new > 4) {
        return 5 - (GetMipBlockHeight(level) >= 2);
    }
    return block_depth_new;
}

std::size_t SurfaceParams::GetGuestMipmapLevelOffset(u32 level) const {
    std::size_t offset = 0;
    for (u32 i = 0; i < level; i++) {
        offset += GetInnerMipmapMemorySize(i, false, false);
    }
    return offset;
}

std::size_t SurfaceParams::GetHostMipmapLevelOffset(u32 level, bool is_converted) const {
    std::size_t offset = 0;
    if (is_converted) {
        for (u32 i = 0; i < level; ++i) {
            offset += GetConvertedMipmapSize(i) * GetNumLayers();
        }
    } else {
        for (u32 i = 0; i < level; ++i) {
            offset += GetInnerMipmapMemorySize(i, true, false) * GetNumLayers();
        }
    }
    return offset;
}

std::size_t SurfaceParams::GetConvertedMipmapSize(u32 level) const {
    constexpr std::size_t rgba8_bpp = 4ULL;
    const std::size_t mip_width = GetMipWidth(level);
    const std::size_t mip_height = GetMipHeight(level);
    const std::size_t mip_depth = is_layered ? 1 : GetMipDepth(level);
    return mip_width * mip_height * mip_depth * rgba8_bpp;
}

std::size_t SurfaceParams::GetLayerSize(bool as_host_size, bool uncompressed) const {
    std::size_t size = 0;
    for (u32 level = 0; level < num_levels; ++level) {
        size += GetInnerMipmapMemorySize(level, as_host_size, uncompressed);
    }
    if (is_tiled && is_layered) {
        return Common::AlignBits(size, Tegra::Texture::GOB_SIZE_SHIFT + block_height + block_depth);
    }
    return size;
}

std::size_t SurfaceParams::GetInnerMipmapMemorySize(u32 level, bool as_host_size,
                                                    bool uncompressed) const {
    const u32 width{GetMipmapSize(uncompressed, GetMipWidth(level), GetDefaultBlockWidth())};
    const u32 height{GetMipmapSize(uncompressed, GetMipHeight(level), GetDefaultBlockHeight())};
    const u32 depth{is_layered ? 1U : GetMipDepth(level)};
    if (is_tiled) {
        return Tegra::Texture::CalculateSize(!as_host_size, GetBytesPerPixel(), width, height,
                                             depth, GetMipBlockHeight(level),
                                             GetMipBlockDepth(level));
    } else if (as_host_size || IsBuffer()) {
        return GetBytesPerPixel() * width * height * depth;
    } else {
        // Linear Texture Case
        return pitch * height * depth;
    }
}

bool SurfaceParams::operator==(const SurfaceParams& rhs) const {
    return std::tie(is_tiled, block_width, block_height, block_depth, tile_width_spacing, width,
                    height, depth, pitch, num_levels, pixel_format, type, target) ==
           std::tie(rhs.is_tiled, rhs.block_width, rhs.block_height, rhs.block_depth,
                    rhs.tile_width_spacing, rhs.width, rhs.height, rhs.depth, rhs.pitch,
                    rhs.num_levels, rhs.pixel_format, rhs.type, rhs.target);
}

std::string SurfaceParams::TargetName() const {
    switch (target) {
    case SurfaceTarget::Texture1D:
        return "1D";
    case SurfaceTarget::TextureBuffer:
        return "TexBuffer";
    case SurfaceTarget::Texture2D:
        return "2D";
    case SurfaceTarget::Texture3D:
        return "3D";
    case SurfaceTarget::Texture1DArray:
        return "1DArray";
    case SurfaceTarget::Texture2DArray:
        return "2DArray";
    case SurfaceTarget::TextureCubemap:
        return "Cube";
    case SurfaceTarget::TextureCubeArray:
        return "CubeArray";
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented surface_target={}", static_cast<u32>(target));
        UNREACHABLE();
        return fmt::format("TUK({})", static_cast<u32>(target));
    }
}

u32 SurfaceParams::GetBlockSize() const {
    const u32 x = 64U << block_width;
    const u32 y = 8U << block_height;
    const u32 z = 1U << block_depth;
    return x * y * z;
}

std::pair<u32, u32> SurfaceParams::GetBlockXY() const {
    const u32 x_pixels = 64U / GetBytesPerPixel();
    const u32 x = x_pixels << block_width;
    const u32 y = 8U << block_height;
    return {x, y};
}

std::tuple<u32, u32, u32> SurfaceParams::GetBlockOffsetXYZ(u32 offset) const {
    const auto div_ceil = [](const u32 x, const u32 y) { return ((x + y - 1) / y); };
    const u32 block_size = GetBlockSize();
    const u32 block_index = offset / block_size;
    const u32 gob_offset = offset % block_size;
    const u32 gob_index = gob_offset / static_cast<u32>(Tegra::Texture::GOB_SIZE);
    const u32 x_gob_pixels = 64U / GetBytesPerPixel();
    const u32 x_block_pixels = x_gob_pixels << block_width;
    const u32 y_block_pixels = 8U << block_height;
    const u32 z_block_pixels = 1U << block_depth;
    const u32 x_blocks = div_ceil(width, x_block_pixels);
    const u32 y_blocks = div_ceil(height, y_block_pixels);
    const u32 z_blocks = div_ceil(depth, z_block_pixels);
    const u32 base_x = block_index % x_blocks;
    const u32 base_y = (block_index / x_blocks) % y_blocks;
    const u32 base_z = (block_index / (x_blocks * y_blocks)) % z_blocks;
    u32 x = base_x * x_block_pixels;
    u32 y = base_y * y_block_pixels;
    u32 z = base_z * z_block_pixels;
    z += gob_index >> block_height;
    y += (gob_index * 8U) % y_block_pixels;
    return {x, y, z};
}

} // namespace VideoCommon
