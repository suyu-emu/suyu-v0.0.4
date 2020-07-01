// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <climits>
#include <utility>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/gpu.h"
#include "video_core/textures/texture.h"

namespace VideoCore::Surface {

enum class PixelFormat {
    A8B8G8R8_UNORM,
    A8B8G8R8_SNORM,
    A8B8G8R8_SINT,
    A8B8G8R8_UINT,
    R5G6B5_UNORM,
    B5G6R5_UNORM,
    A1R5G5B5_UNORM,
    A2B10G10R10_UNORM,
    A2B10G10R10_UINT,
    A1B5G5R5_UNORM,
    R8_UNORM,
    R8_SNORM,
    R8_SINT,
    R8_UINT,
    R16G16B16A16_FLOAT,
    R16G16B16A16_UNORM,
    R16G16B16A16_SNORM,
    R16G16B16A16_SINT,
    R16G16B16A16_UINT,
    B10G11R11_FLOAT,
    R32G32B32A32_UINT,
    BC1_RGBA_UNORM,
    BC2_UNORM,
    BC3_UNORM,
    BC4_UNORM,
    BC4_SNORM,
    BC5_UNORM,
    BC5_SNORM,
    BC7_UNORM,
    BC6H_UFLOAT,
    BC6H_SFLOAT,
    ASTC_2D_4X4_UNORM,
    B8G8R8A8_UNORM,
    R32G32B32A32_FLOAT,
    R32G32B32A32_SINT,
    R32G32_FLOAT,
    R32G32_SINT,
    R32_FLOAT,
    R16_FLOAT,
    R16_UNORM,
    R16_SNORM,
    R16_UINT,
    R16_SINT,
    R16G16_UNORM,
    R16G16_FLOAT,
    R16G16_UINT,
    R16G16_SINT,
    R16G16_SNORM,
    R32G32B32_FLOAT,
    A8B8G8R8_SRGB,
    R8G8_UNORM,
    R8G8_SNORM,
    R8G8_SINT,
    R8G8_UINT,
    R32G32_UINT,
    R16G16B16X16_FLOAT,
    R32_UINT,
    R32_SINT,
    ASTC_2D_8X8_UNORM,
    ASTC_2D_8X5_UNORM,
    ASTC_2D_5X4_UNORM,
    B8G8R8A8_SRGB,
    BC1_RGBA_SRGB,
    BC2_SRGB,
    BC3_SRGB,
    BC7_SRGB,
    A4B4G4R4_UNORM,
    ASTC_2D_4X4_SRGB,
    ASTC_2D_8X8_SRGB,
    ASTC_2D_8X5_SRGB,
    ASTC_2D_5X4_SRGB,
    ASTC_2D_5X5_UNORM,
    ASTC_2D_5X5_SRGB,
    ASTC_2D_10X8_UNORM,
    ASTC_2D_10X8_SRGB,
    ASTC_2D_6X6_UNORM,
    ASTC_2D_6X6_SRGB,
    ASTC_2D_10X10_UNORM,
    ASTC_2D_10X10_SRGB,
    ASTC_2D_12X12_UNORM,
    ASTC_2D_12X12_SRGB,
    ASTC_2D_8X6_UNORM,
    ASTC_2D_8X6_SRGB,
    ASTC_2D_6X5_UNORM,
    ASTC_2D_6X5_SRGB,
    E5B9G9R9_FLOAT,

    MaxColorFormat,

    // Depth formats
    D32_FLOAT = MaxColorFormat,
    D16_UNORM,

    MaxDepthFormat,

    // DepthStencil formats
    D24_UNORM_S8_UINT = MaxDepthFormat,
    S8_UINT_D24_UNORM,
    D32_FLOAT_S8_UINT,

    MaxDepthStencilFormat,

    Max = MaxDepthStencilFormat,
    Invalid = 255,
};
static constexpr std::size_t MaxPixelFormat = static_cast<std::size_t>(PixelFormat::Max);

enum class SurfaceType {
    ColorTexture = 0,
    Depth = 1,
    DepthStencil = 2,
    Invalid = 3,
};

enum class SurfaceTarget {
    Texture1D,
    TextureBuffer,
    Texture2D,
    Texture3D,
    Texture1DArray,
    Texture2DArray,
    TextureCubemap,
    TextureCubeArray,
};

constexpr std::array<u32, MaxPixelFormat> compression_factor_shift_table = {{
    0, // A8B8G8R8_UNORM
    0, // A8B8G8R8_SNORM
    0, // A8B8G8R8_SINT
    0, // A8B8G8R8_UINT
    0, // R5G6B5_UNORM
    0, // B5G6R5_UNORM
    0, // A1R5G5B5_UNORM
    0, // A2B10G10R10_UNORM
    0, // A2B10G10R10_UINT
    0, // A1B5G5R5_UNORM
    0, // R8_UNORM
    0, // R8_SNORM
    0, // R8_SINT
    0, // R8_UINT
    0, // R16G16B16A16_FLOAT
    0, // R16G16B16A16_UNORM
    0, // R16G16B16A16_SNORM
    0, // R16G16B16A16_SINT
    0, // R16G16B16A16_UINT
    0, // B10G11R11_FLOAT
    0, // R32G32B32A32_UINT
    2, // BC1_RGBA_UNORM
    2, // BC2_UNORM
    2, // BC3_UNORM
    2, // BC4_UNORM
    2, // BC4_SNORM
    2, // BC5_UNORM
    2, // BC5_SNORM
    2, // BC7_UNORM
    2, // BC6H_UFLOAT
    2, // BC6H_SFLOAT
    2, // ASTC_2D_4X4_UNORM
    0, // B8G8R8A8_UNORM
    0, // R32G32B32A32_FLOAT
    0, // R32G32B32A32_SINT
    0, // R32G32_FLOAT
    0, // R32G32_SINT
    0, // R32_FLOAT
    0, // R16_FLOAT
    0, // R16_UNORM
    0, // R16_SNORM
    0, // R16_UINT
    0, // R16_SINT
    0, // R16G16_UNORM
    0, // R16G16_FLOAT
    0, // R16G16_UINT
    0, // R16G16_SINT
    0, // R16G16_SNORM
    0, // R32G32B32_FLOAT
    0, // A8B8G8R8_SRGB
    0, // R8G8_UNORM
    0, // R8G8_SNORM
    0, // R8G8_SINT
    0, // R8G8_UINT
    0, // R32G32_UINT
    0, // R16G16B16X16_FLOAT
    0, // R32_UINT
    0, // R32_SINT
    2, // ASTC_2D_8X8_UNORM
    2, // ASTC_2D_8X5_UNORM
    2, // ASTC_2D_5X4_UNORM
    0, // B8G8R8A8_SRGB
    2, // BC1_RGBA_SRGB
    2, // BC2_SRGB
    2, // BC3_SRGB
    2, // BC7_SRGB
    0, // A4B4G4R4_UNORM
    2, // ASTC_2D_4X4_SRGB
    2, // ASTC_2D_8X8_SRGB
    2, // ASTC_2D_8X5_SRGB
    2, // ASTC_2D_5X4_SRGB
    2, // ASTC_2D_5X5_UNORM
    2, // ASTC_2D_5X5_SRGB
    2, // ASTC_2D_10X8_UNORM
    2, // ASTC_2D_10X8_SRGB
    2, // ASTC_2D_6X6_UNORM
    2, // ASTC_2D_6X6_SRGB
    2, // ASTC_2D_10X10_UNORM
    2, // ASTC_2D_10X10_SRGB
    2, // ASTC_2D_12X12_UNORM
    2, // ASTC_2D_12X12_SRGB
    2, // ASTC_2D_8X6_UNORM
    2, // ASTC_2D_8X6_SRGB
    2, // ASTC_2D_6X5_UNORM
    2, // ASTC_2D_6X5_SRGB
    0, // E5B9G9R9_FLOAT
    0, // D32_FLOAT
    0, // D16_UNORM
    0, // D24_UNORM_S8_UINT
    0, // S8_UINT_D24_UNORM
    0, // D32_FLOAT_S8_UINT
}};

/**
 * Gets the compression factor for the specified PixelFormat. This applies to just the
 * "compressed width" and "compressed height", not the overall compression factor of a
 * compressed image. This is used for maintaining proper surface sizes for compressed
 * texture formats.
 */
inline constexpr u32 GetCompressionFactorShift(PixelFormat format) {
    DEBUG_ASSERT(format != PixelFormat::Invalid);
    DEBUG_ASSERT(static_cast<std::size_t>(format) < compression_factor_shift_table.size());
    return compression_factor_shift_table[static_cast<std::size_t>(format)];
}

inline constexpr u32 GetCompressionFactor(PixelFormat format) {
    return 1U << GetCompressionFactorShift(format);
}

constexpr std::array<u32, MaxPixelFormat> block_width_table = {{
    1,  // A8B8G8R8_UNORM
    1,  // A8B8G8R8_SNORM
    1,  // A8B8G8R8_SINT
    1,  // A8B8G8R8_UINT
    1,  // R5G6B5_UNORM
    1,  // B5G6R5_UNORM
    1,  // A1R5G5B5_UNORM
    1,  // A2B10G10R10_UNORM
    1,  // A2B10G10R10_UINT
    1,  // A1B5G5R5_UNORM
    1,  // R8_UNORM
    1,  // R8_SNORM
    1,  // R8_SINT
    1,  // R8_UINT
    1,  // R16G16B16A16_FLOAT
    1,  // R16G16B16A16_UNORM
    1,  // R16G16B16A16_SNORM
    1,  // R16G16B16A16_SINT
    1,  // R16G16B16A16_UINT
    1,  // B10G11R11_FLOAT
    1,  // R32G32B32A32_UINT
    4,  // BC1_RGBA_UNORM
    4,  // BC2_UNORM
    4,  // BC3_UNORM
    4,  // BC4_UNORM
    4,  // BC4_SNORM
    4,  // BC5_UNORM
    4,  // BC5_SNORM
    4,  // BC7_UNORM
    4,  // BC6H_UFLOAT
    4,  // BC6H_SFLOAT
    4,  // ASTC_2D_4X4_UNORM
    1,  // B8G8R8A8_UNORM
    1,  // R32G32B32A32_FLOAT
    1,  // R32G32B32A32_SINT
    1,  // R32G32_FLOAT
    1,  // R32G32_SINT
    1,  // R32_FLOAT
    1,  // R16_FLOAT
    1,  // R16_UNORM
    1,  // R16_SNORM
    1,  // R16_UINT
    1,  // R16_SINT
    1,  // R16G16_UNORM
    1,  // R16G16_FLOAT
    1,  // R16G16_UINT
    1,  // R16G16_SINT
    1,  // R16G16_SNORM
    1,  // R32G32B32_FLOAT
    1,  // A8B8G8R8_SRGB
    1,  // R8G8_UNORM
    1,  // R8G8_SNORM
    1,  // R8G8_SINT
    1,  // R8G8_UINT
    1,  // R32G32_UINT
    1,  // R16G16B16X16_FLOAT
    1,  // R32_UINT
    1,  // R32_SINT
    8,  // ASTC_2D_8X8_UNORM
    8,  // ASTC_2D_8X5_UNORM
    5,  // ASTC_2D_5X4_UNORM
    1,  // B8G8R8A8_SRGB
    4,  // BC1_RGBA_SRGB
    4,  // BC2_SRGB
    4,  // BC3_SRGB
    4,  // BC7_SRGB
    1,  // A4B4G4R4_UNORM
    4,  // ASTC_2D_4X4_SRGB
    8,  // ASTC_2D_8X8_SRGB
    8,  // ASTC_2D_8X5_SRGB
    5,  // ASTC_2D_5X4_SRGB
    5,  // ASTC_2D_5X5_UNORM
    5,  // ASTC_2D_5X5_SRGB
    10, // ASTC_2D_10X8_UNORM
    10, // ASTC_2D_10X8_SRGB
    6,  // ASTC_2D_6X6_UNORM
    6,  // ASTC_2D_6X6_SRGB
    10, // ASTC_2D_10X10_UNORM
    10, // ASTC_2D_10X10_SRGB
    12, // ASTC_2D_12X12_UNORM
    12, // ASTC_2D_12X12_SRGB
    8,  // ASTC_2D_8X6_UNORM
    8,  // ASTC_2D_8X6_SRGB
    6,  // ASTC_2D_6X5_UNORM
    6,  // ASTC_2D_6X5_SRGB
    1,  // E5B9G9R9_FLOAT
    1,  // D32_FLOAT
    1,  // D16_UNORM
    1,  // D24_UNORM_S8_UINT
    1,  // S8_UINT_D24_UNORM
    1,  // D32_FLOAT_S8_UINT
}};

static constexpr u32 GetDefaultBlockWidth(PixelFormat format) {
    if (format == PixelFormat::Invalid)
        return 0;

    ASSERT(static_cast<std::size_t>(format) < block_width_table.size());
    return block_width_table[static_cast<std::size_t>(format)];
}

constexpr std::array<u32, MaxPixelFormat> block_height_table = {{
    1,  // A8B8G8R8_UNORM
    1,  // A8B8G8R8_SNORM
    1,  // A8B8G8R8_SINT
    1,  // A8B8G8R8_UINT
    1,  // R5G6B5_UNORM
    1,  // B5G6R5_UNORM
    1,  // A1R5G5B5_UNORM
    1,  // A2B10G10R10_UNORM
    1,  // A2B10G10R10_UINT
    1,  // A1B5G5R5_UNORM
    1,  // R8_UNORM
    1,  // R8_SNORM
    1,  // R8_SINT
    1,  // R8_UINT
    1,  // R16G16B16A16_FLOAT
    1,  // R16G16B16A16_UNORM
    1,  // R16G16B16A16_SNORM
    1,  // R16G16B16A16_SINT
    1,  // R16G16B16A16_UINT
    1,  // B10G11R11_FLOAT
    1,  // R32G32B32A32_UINT
    4,  // BC1_RGBA_UNORM
    4,  // BC2_UNORM
    4,  // BC3_UNORM
    4,  // BC4_UNORM
    4,  // BC4_SNORM
    4,  // BC5_UNORM
    4,  // BC5_SNORM
    4,  // BC7_UNORM
    4,  // BC6H_UFLOAT
    4,  // BC6H_SFLOAT
    4,  // ASTC_2D_4X4_UNORM
    1,  // B8G8R8A8_UNORM
    1,  // R32G32B32A32_FLOAT
    1,  // R32G32B32A32_SINT
    1,  // R32G32_FLOAT
    1,  // R32G32_SINT
    1,  // R32_FLOAT
    1,  // R16_FLOAT
    1,  // R16_UNORM
    1,  // R16_SNORM
    1,  // R16_UINT
    1,  // R16_SINT
    1,  // R16G16_UNORM
    1,  // R16G16_FLOAT
    1,  // R16G16_UINT
    1,  // R16G16_SINT
    1,  // R16G16_SNORM
    1,  // R32G32B32_FLOAT
    1,  // A8B8G8R8_SRGB
    1,  // R8G8_UNORM
    1,  // R8G8_SNORM
    1,  // R8G8_SINT
    1,  // R8G8_UINT
    1,  // R32G32_UINT
    1,  // R16G16B16X16_FLOAT
    1,  // R32_UINT
    1,  // R32_SINT
    8,  // ASTC_2D_8X8_UNORM
    5,  // ASTC_2D_8X5_UNORM
    4,  // ASTC_2D_5X4_UNORM
    1,  // B8G8R8A8_SRGB
    4,  // BC1_RGBA_SRGB
    4,  // BC2_SRGB
    4,  // BC3_SRGB
    4,  // BC7_SRGB
    1,  // A4B4G4R4_UNORM
    4,  // ASTC_2D_4X4_SRGB
    8,  // ASTC_2D_8X8_SRGB
    5,  // ASTC_2D_8X5_SRGB
    4,  // ASTC_2D_5X4_SRGB
    5,  // ASTC_2D_5X5_UNORM
    5,  // ASTC_2D_5X5_SRGB
    8,  // ASTC_2D_10X8_UNORM
    8,  // ASTC_2D_10X8_SRGB
    6,  // ASTC_2D_6X6_UNORM
    6,  // ASTC_2D_6X6_SRGB
    10, // ASTC_2D_10X10_UNORM
    10, // ASTC_2D_10X10_SRGB
    12, // ASTC_2D_12X12_UNORM
    12, // ASTC_2D_12X12_SRGB
    6,  // ASTC_2D_8X6_UNORM
    6,  // ASTC_2D_8X6_SRGB
    5,  // ASTC_2D_6X5_UNORM
    5,  // ASTC_2D_6X5_SRGB
    1,  // E5B9G9R9_FLOAT
    1,  // D32_FLOAT
    1,  // D16_UNORM
    1,  // D24_UNORM_S8_UINT
    1,  // S8_UINT_D24_UNORM
    1,  // D32_FLOAT_S8_UINT
}};

static constexpr u32 GetDefaultBlockHeight(PixelFormat format) {
    if (format == PixelFormat::Invalid)
        return 0;

    ASSERT(static_cast<std::size_t>(format) < block_height_table.size());
    return block_height_table[static_cast<std::size_t>(format)];
}

constexpr std::array<u32, MaxPixelFormat> bpp_table = {{
    32,  // A8B8G8R8_UNORM
    32,  // A8B8G8R8_SNORM
    32,  // A8B8G8R8_SINT
    32,  // A8B8G8R8_UINT
    16,  // R5G6B5_UNORM
    16,  // B5G6R5_UNORM
    16,  // A1R5G5B5_UNORM
    32,  // A2B10G10R10_UNORM
    32,  // A2B10G10R10_UINT
    16,  // A1B5G5R5_UNORM
    8,   // R8_UNORM
    8,   // R8_SNORM
    8,   // R8_SINT
    8,   // R8_UINT
    64,  // R16G16B16A16_FLOAT
    64,  // R16G16B16A16_UNORM
    64,  // R16G16B16A16_SNORM
    64,  // R16G16B16A16_SINT
    64,  // R16G16B16A16_UINT
    32,  // B10G11R11_FLOAT
    128, // R32G32B32A32_UINT
    64,  // BC1_RGBA_UNORM
    128, // BC2_UNORM
    128, // BC3_UNORM
    64,  // BC4_UNORM
    64,  // BC4_SNORM
    128, // BC5_UNORM
    128, // BC5_SNORM
    128, // BC7_UNORM
    128, // BC6H_UFLOAT
    128, // BC6H_SFLOAT
    128, // ASTC_2D_4X4_UNORM
    32,  // B8G8R8A8_UNORM
    128, // R32G32B32A32_FLOAT
    128, // R32G32B32A32_SINT
    64,  // R32G32_FLOAT
    64,  // R32G32_SINT
    32,  // R32_FLOAT
    16,  // R16_FLOAT
    16,  // R16_UNORM
    16,  // R16_SNORM
    16,  // R16_UINT
    16,  // R16_SINT
    32,  // R16G16_UNORM
    32,  // R16G16_FLOAT
    32,  // R16G16_UINT
    32,  // R16G16_SINT
    32,  // R16G16_SNORM
    96,  // R32G32B32_FLOAT
    32,  // A8B8G8R8_SRGB
    16,  // R8G8_UNORM
    16,  // R8G8_SNORM
    16,  // R8G8_SINT
    16,  // R8G8_UINT
    64,  // R32G32_UINT
    64,  // R16G16B16X16_FLOAT
    32,  // R32_UINT
    32,  // R32_SINT
    128, // ASTC_2D_8X8_UNORM
    128, // ASTC_2D_8X5_UNORM
    128, // ASTC_2D_5X4_UNORM
    32,  // B8G8R8A8_SRGB
    64,  // BC1_RGBA_SRGB
    128, // BC2_SRGB
    128, // BC3_SRGB
    128, // BC7_UNORM
    16,  // A4B4G4R4_UNORM
    128, // ASTC_2D_4X4_SRGB
    128, // ASTC_2D_8X8_SRGB
    128, // ASTC_2D_8X5_SRGB
    128, // ASTC_2D_5X4_SRGB
    128, // ASTC_2D_5X5_UNORM
    128, // ASTC_2D_5X5_SRGB
    128, // ASTC_2D_10X8_UNORM
    128, // ASTC_2D_10X8_SRGB
    128, // ASTC_2D_6X6_UNORM
    128, // ASTC_2D_6X6_SRGB
    128, // ASTC_2D_10X10_UNORM
    128, // ASTC_2D_10X10_SRGB
    128, // ASTC_2D_12X12_UNORM
    128, // ASTC_2D_12X12_SRGB
    128, // ASTC_2D_8X6_UNORM
    128, // ASTC_2D_8X6_SRGB
    128, // ASTC_2D_6X5_UNORM
    128, // ASTC_2D_6X5_SRGB
    32,  // E5B9G9R9_FLOAT
    32,  // D32_FLOAT
    16,  // D16_UNORM
    32,  // D24_UNORM_S8_UINT
    32,  // S8_UINT_D24_UNORM
    64,  // D32_FLOAT_S8_UINT
}};

static constexpr u32 GetFormatBpp(PixelFormat format) {
    if (format == PixelFormat::Invalid)
        return 0;

    ASSERT(static_cast<std::size_t>(format) < bpp_table.size());
    return bpp_table[static_cast<std::size_t>(format)];
}

/// Returns the sizer in bytes of the specified pixel format
static constexpr u32 GetBytesPerPixel(PixelFormat pixel_format) {
    if (pixel_format == PixelFormat::Invalid) {
        return 0;
    }
    return GetFormatBpp(pixel_format) / CHAR_BIT;
}

SurfaceTarget SurfaceTargetFromTextureType(Tegra::Texture::TextureType texture_type);

bool SurfaceTargetIsLayered(SurfaceTarget target);

bool SurfaceTargetIsArray(SurfaceTarget target);

PixelFormat PixelFormatFromDepthFormat(Tegra::DepthFormat format);

PixelFormat PixelFormatFromRenderTargetFormat(Tegra::RenderTargetFormat format);

PixelFormat PixelFormatFromGPUPixelFormat(Tegra::FramebufferConfig::PixelFormat format);

SurfaceType GetFormatType(PixelFormat pixel_format);

bool IsPixelFormatASTC(PixelFormat format);

bool IsPixelFormatSRGB(PixelFormat format);

std::pair<u32, u32> GetASTCBlockSize(PixelFormat format);

} // namespace VideoCore::Surface
