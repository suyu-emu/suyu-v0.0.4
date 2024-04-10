// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/types.h"

namespace Metal::MaxwellToMTL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct PixelFormatInfo {
    MTL::PixelFormat pixel_format;
    size_t bytes_per_block;
    VideoCommon::Extent2D block_texel_size{1, 1};
};

// TODO: replace some of the invalid formats with the correct ones and emulate those which don't map
// directly
// TODO: set the sizes of other formats as well
constexpr std::array<PixelFormatInfo, VideoCore::Surface::MaxPixelFormat> FORMAT_TABLE = {{
    {MTL::PixelFormatInvalid, 0},                // A8B8G8R8_UNORM
    {MTL::PixelFormatInvalid, 0},                // A8B8G8R8_SNORM
    {MTL::PixelFormatInvalid, 0},                // A8B8G8R8_SINT
    {MTL::PixelFormatInvalid, 0},                // A8B8G8R8_UINT
    {MTL::PixelFormatInvalid, 0},                // R5G6B5_UNORM
    {MTL::PixelFormatB5G6R5Unorm, 2},            // B5G6R5_UNORM
    {MTL::PixelFormatInvalid, 0},                // A1R5G5B5_UNORM
    {MTL::PixelFormatRGBA8Unorm, 4},             // A2B10G10R10_UNORM (hack)
    {MTL::PixelFormatInvalid, 0},                // A2B10G10R10_UINT
    {MTL::PixelFormatInvalid, 0},                // A2R10G10B10_UNORM
    {MTL::PixelFormatInvalid, 0},                // A1B5G5R5_UNORM
    {MTL::PixelFormatInvalid, 0},                // A5B5G5R1_UNORM
    {MTL::PixelFormatR8Unorm, 1},                // R8_UNORM
    {MTL::PixelFormatR8Snorm, 1},                // R8_SNORM
    {MTL::PixelFormatR8Sint, 1},                 // R8_SINT
    {MTL::PixelFormatR8Uint, 1},                 // R8_UINT
    {MTL::PixelFormatRGBA16Float, 8},            // R16G16B16A16_FLOAT
    {MTL::PixelFormatRGBA16Unorm, 8},            // R16G16B16A16_UNORM
    {MTL::PixelFormatRGBA16Snorm, 8},            // R16G16B16A16_SNORM
    {MTL::PixelFormatRGBA16Sint, 8},             // R16G16B16A16_SINT
    {MTL::PixelFormatRGBA16Uint, 8},             // R16G16B16A16_UINT
    {MTL::PixelFormatInvalid, 0},                // B10G11R11_FLOAT
    {MTL::PixelFormatRGBA32Uint, 16},            // R32G32B32A32_UINT
    {MTL::PixelFormatBC1_RGBA, 8, {4, 4}},       // BC1_RGBA_UNORM
    {MTL::PixelFormatInvalid, 0},                // BC2_UNORM
    {MTL::PixelFormatInvalid, 0},                // BC3_UNORM
    {MTL::PixelFormatBC4_RUnorm, 8, {4, 4}},     // BC4_UNORM
    {MTL::PixelFormatBC4_RSnorm, 0},             // BC4_SNORM
    {MTL::PixelFormatInvalid, 0},                // BC5_UNORM
    {MTL::PixelFormatInvalid, 0},                // BC5_SNORM
    {MTL::PixelFormatInvalid, 0},                // BC7_UNORM
    {MTL::PixelFormatInvalid, 0},                // BC6H_UFLOAT
    {MTL::PixelFormatInvalid, 0},                // BC6H_SFLOAT
    {MTL::PixelFormatASTC_4x4_LDR, 0},           // ASTC_2D_4X4_UNORM
    {MTL::PixelFormatBGRA8Unorm, 4},             // B8G8R8A8_UNORM
    {MTL::PixelFormatRGBA32Float, 16},           // R32G32B32A32_FLOAT
    {MTL::PixelFormatRGBA32Sint, 16},            // R32G32B32A32_SINT
    {MTL::PixelFormatRG32Float, 8},              // R32G32_FLOAT
    {MTL::PixelFormatRG32Sint, 8},               // R32G32_SINT
    {MTL::PixelFormatR32Float, 4},               // R32_FLOAT
    {MTL::PixelFormatR16Float, 2},               // R16_FLOAT
    {MTL::PixelFormatR16Unorm, 2},               // R16_UNORM
    {MTL::PixelFormatR16Snorm, 2},               // R16_SNORM
    {MTL::PixelFormatR16Uint, 2},                // R16_UINT
    {MTL::PixelFormatR16Sint, 2},                // R16_SINT
    {MTL::PixelFormatRG16Unorm, 4},              // R16G16_UNORM
    {MTL::PixelFormatRG16Float, 4},              // R16G16_FLOAT
    {MTL::PixelFormatRG16Uint, 4},               // R16G16_UINT
    {MTL::PixelFormatRG16Sint, 4},               // R16G16_SINT
    {MTL::PixelFormatRG16Snorm, 4},              // R16G16_SNORM
    {MTL::PixelFormatInvalid, 0},                // R32G32B32_FLOAT
    {MTL::PixelFormatRGBA8Unorm, 4},             // A8B8G8R8_SRGB
    {MTL::PixelFormatRG8Unorm, 2},               // R8G8_UNORM
    {MTL::PixelFormatRG8Snorm, 2},               // R8G8_SNORM
    {MTL::PixelFormatRG8Sint, 2},                // R8G8_SINT
    {MTL::PixelFormatRG8Uint, 2},                // R8G8_UINT
    {MTL::PixelFormatRG32Uint, 8},               // R32G32_UINT
    {MTL::PixelFormatInvalid, 0},                // R16G16B16X16_FLOAT
    {MTL::PixelFormatR32Uint, 4},                // R32_UINT
    {MTL::PixelFormatR32Sint, 4},                // R32_SINT
    {MTL::PixelFormatASTC_8x8_LDR, 16, {8, 8}},  // ASTC_2D_8X8_UNORM
    {MTL::PixelFormatASTC_8x5_LDR, 0},           // ASTC_2D_8X5_UNORM
    {MTL::PixelFormatASTC_5x4_LDR, 0},           // ASTC_2D_5X4_UNORM
    {MTL::PixelFormatBGRA8Unorm_sRGB, 0},        // B8G8R8A8_SRGB
    {MTL::PixelFormatBC1_RGBA_sRGB, 0},          // BC1_RGBA_SRGB
    {MTL::PixelFormatInvalid, 0},                // BC2_SRGB
    {MTL::PixelFormatInvalid, 0},                // BC3_SRGB
    {MTL::PixelFormatBC7_RGBAUnorm_sRGB, 0},     // BC7_SRGB
    {MTL::PixelFormatABGR4Unorm, 0},             // A4B4G4R4_UNORM
    {MTL::PixelFormatInvalid, 0},                // G4R4_UNORM
    {MTL::PixelFormatASTC_4x4_sRGB, 16, {4, 4}}, // ASTC_2D_4X4_SRGB
    {MTL::PixelFormatASTC_8x8_sRGB, 0},          // ASTC_2D_8X8_SRGB
    {MTL::PixelFormatASTC_8x5_sRGB, 0},          // ASTC_2D_8X5_SRGB
    {MTL::PixelFormatASTC_5x4_sRGB, 0},          // ASTC_2D_5X4_SRGB
    {MTL::PixelFormatASTC_5x5_LDR, 0},           // ASTC_2D_5X5_UNORM
    {MTL::PixelFormatASTC_5x5_sRGB, 0},          // ASTC_2D_5X5_SRGB
    {MTL::PixelFormatASTC_10x8_LDR, 0},          // ASTC_2D_10X8_UNORM
    {MTL::PixelFormatASTC_10x8_sRGB, 0},         // ASTC_2D_10X8_SRGB
    {MTL::PixelFormatASTC_6x6_LDR, 0},           // ASTC_2D_6X6_UNORM
    {MTL::PixelFormatASTC_6x6_sRGB, 0},          // ASTC_2D_6X6_SRGB
    {MTL::PixelFormatASTC_10x6_LDR, 0},          // ASTC_2D_10X6_UNORM
    {MTL::PixelFormatASTC_10x6_sRGB, 0},         // ASTC_2D_10X6_SRGB
    {MTL::PixelFormatASTC_10x5_LDR, 0},          // ASTC_2D_10X5_UNORM
    {MTL::PixelFormatASTC_10x5_sRGB, 0},         // ASTC_2D_10X5_SRGB
    {MTL::PixelFormatASTC_10x10_LDR, 0},         // ASTC_2D_10X10_UNORM
    {MTL::PixelFormatASTC_10x10_sRGB, 0},        // ASTC_2D_10X10_SRGB
    {MTL::PixelFormatASTC_12x10_LDR, 0},         // ASTC_2D_12X10_UNORM
    {MTL::PixelFormatASTC_12x10_sRGB, 0},        // ASTC_2D_12X10_SRGB
    {MTL::PixelFormatASTC_12x12_LDR, 0},         // ASTC_2D_12X12_UNORM
    {MTL::PixelFormatASTC_12x12_sRGB, 0},        // ASTC_2D_12X12_SRGB
    {MTL::PixelFormatASTC_8x6_LDR, 0},           // ASTC_2D_8X6_UNORM
    {MTL::PixelFormatASTC_8x6_sRGB, 0},          // ASTC_2D_8X6_SRGB
    {MTL::PixelFormatASTC_6x5_LDR, 0},           // ASTC_2D_6X5_UNORM
    {MTL::PixelFormatASTC_6x5_sRGB, 0},          // ASTC_2D_6X5_SRGB
    {MTL::PixelFormatInvalid, 0},                // E5B9G9R9_FLOAT
    {MTL::PixelFormatDepth32Float, 0},           // D32_FLOAT
    {MTL::PixelFormatDepth16Unorm, 0},           // D16_UNORM
    {MTL::PixelFormatInvalid, 0},                // X8_D24_UNORM
    {MTL::PixelFormatStencil8, 0},               // S8_UINT
    {MTL::PixelFormatDepth24Unorm_Stencil8, 0},  // D24_UNORM_S8_UINT
    {MTL::PixelFormatInvalid, 0},                // S8_UINT_D24_UNORM
    {MTL::PixelFormatDepth32Float_Stencil8, 0},  // D32_FLOAT_S8_UINT
}};

inline const PixelFormatInfo GetPixelFormatInfo(VideoCore::Surface::PixelFormat pixel_format) {
    ASSERT(static_cast<size_t>(pixel_format) < FORMAT_TABLE.size());

    return FORMAT_TABLE[static_cast<size_t>(pixel_format)];
}

inline u32 CeilDivide(u32 a, u32 b) {
    return (a + b - 1) / b;
}

inline size_t GetTextureBytesPerRow(VideoCore::Surface::PixelFormat pixel_format,
                                    u32 texels_per_row) {
    const auto& format_info = GetPixelFormatInfo(pixel_format);

    return CeilDivide(texels_per_row, format_info.block_texel_size.width) *
           format_info.bytes_per_block;
}

} // namespace Metal::MaxwellToMTL
