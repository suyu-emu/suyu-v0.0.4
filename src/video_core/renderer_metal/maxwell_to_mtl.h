// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Metal/Metal.hpp>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/surface.h"

namespace Metal::MaxwellToMTL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

// TODO: replace some of the invalid formats with the correct ones and emulate those which don't map
// directly
constexpr std::array<MTL::PixelFormat, VideoCore::Surface::MaxPixelFormat> FORMAT_TABLE = {
    MTL::PixelFormatInvalid,               // A8B8G8R8_UNORM
    MTL::PixelFormatInvalid,               // A8B8G8R8_SNORM
    MTL::PixelFormatInvalid,               // A8B8G8R8_SINT
    MTL::PixelFormatInvalid,               // A8B8G8R8_UINT
    MTL::PixelFormatInvalid,               // R5G6B5_UNORM
    MTL::PixelFormatB5G6R5Unorm,           // B5G6R5_UNORM
    MTL::PixelFormatInvalid,               // A1R5G5B5_UNORM
    MTL::PixelFormatRGBA8Unorm,            // A2B10G10R10_UNORM (hack)
    MTL::PixelFormatInvalid,               // A2B10G10R10_UINT
    MTL::PixelFormatInvalid,               // A2R10G10B10_UNORM
    MTL::PixelFormatInvalid,               // A1B5G5R5_UNORM
    MTL::PixelFormatInvalid,               // A5B5G5R1_UNORM
    MTL::PixelFormatR8Unorm,               // R8_UNORM
    MTL::PixelFormatR8Snorm,               // R8_SNORM
    MTL::PixelFormatR8Sint,                // R8_SINT
    MTL::PixelFormatR8Uint,                // R8_UINT
    MTL::PixelFormatRGBA16Float,           // R16G16B16A16_FLOAT
    MTL::PixelFormatRGBA16Unorm,           // R16G16B16A16_UNORM
    MTL::PixelFormatRGBA16Snorm,           // R16G16B16A16_SNORM
    MTL::PixelFormatRGBA16Sint,            // R16G16B16A16_SINT
    MTL::PixelFormatRGBA16Uint,            // R16G16B16A16_UINT
    MTL::PixelFormatInvalid,               // B10G11R11_FLOAT
    MTL::PixelFormatRGBA32Uint,            // R32G32B32A32_UINT
    MTL::PixelFormatBC1_RGBA,              // BC1_RGBA_UNORM
    MTL::PixelFormatInvalid,               // BC2_UNORM
    MTL::PixelFormatInvalid,               // BC3_UNORM
    MTL::PixelFormatBC4_RUnorm,            // BC4_UNORM
    MTL::PixelFormatBC4_RSnorm,            // BC4_SNORM
    MTL::PixelFormatInvalid,               // BC5_UNORM
    MTL::PixelFormatInvalid,               // BC5_SNORM
    MTL::PixelFormatInvalid,               // BC7_UNORM
    MTL::PixelFormatInvalid,               // BC6H_UFLOAT
    MTL::PixelFormatInvalid,               // BC6H_SFLOAT
    MTL::PixelFormatASTC_4x4_LDR,          // ASTC_2D_4X4_UNORM
    MTL::PixelFormatBGRA8Unorm,            // B8G8R8A8_UNORM
    MTL::PixelFormatRGBA32Float,           // R32G32B32A32_FLOAT
    MTL::PixelFormatRGBA32Sint,            // R32G32B32A32_SINT
    MTL::PixelFormatRG32Float,             // R32G32_FLOAT
    MTL::PixelFormatRG32Sint,              // R32G32_SINT
    MTL::PixelFormatR32Float,              // R32_FLOAT
    MTL::PixelFormatR16Float,              // R16_FLOAT
    MTL::PixelFormatR16Unorm,              // R16_UNORM
    MTL::PixelFormatR16Snorm,              // R16_SNORM
    MTL::PixelFormatR16Uint,               // R16_UINT
    MTL::PixelFormatR16Sint,               // R16_SINT
    MTL::PixelFormatRG16Unorm,             // R16G16_UNORM
    MTL::PixelFormatRG16Float,             // R16G16_FLOAT
    MTL::PixelFormatRG16Uint,              // R16G16_UINT
    MTL::PixelFormatRG16Sint,              // R16G16_SINT
    MTL::PixelFormatRG16Snorm,             // R16G16_SNORM
    MTL::PixelFormatInvalid,               // R32G32B32_FLOAT
    MTL::PixelFormatRGBA8Unorm,            // A8B8G8R8_SRGB (hack)
    MTL::PixelFormatRG8Unorm,              // R8G8_UNORM
    MTL::PixelFormatRG8Snorm,              // R8G8_SNORM
    MTL::PixelFormatRG8Sint,               // R8G8_SINT
    MTL::PixelFormatRG8Uint,               // R8G8_UINT
    MTL::PixelFormatRG32Uint,              // R32G32_UINT
    MTL::PixelFormatInvalid,               // R16G16B16X16_FLOAT
    MTL::PixelFormatR32Uint,               // R32_UINT
    MTL::PixelFormatR32Sint,               // R32_SINT
    MTL::PixelFormatASTC_8x8_LDR,          // ASTC_2D_8X8_UNORM
    MTL::PixelFormatASTC_8x5_LDR,          // ASTC_2D_8X5_UNORM
    MTL::PixelFormatASTC_5x4_LDR,          // ASTC_2D_5X4_UNORM
    MTL::PixelFormatBGRA8Unorm_sRGB,       // B8G8R8A8_SRGB
    MTL::PixelFormatBC1_RGBA_sRGB,         // BC1_RGBA_SRGB
    MTL::PixelFormatInvalid,               // BC2_SRGB
    MTL::PixelFormatInvalid,               // BC3_SRGB
    MTL::PixelFormatBC7_RGBAUnorm_sRGB,    // BC7_SRGB
    MTL::PixelFormatABGR4Unorm,            // A4B4G4R4_UNORM
    MTL::PixelFormatInvalid,               // G4R4_UNORM
    MTL::PixelFormatASTC_4x4_sRGB,         // ASTC_2D_4X4_SRGB
    MTL::PixelFormatASTC_8x8_sRGB,         // ASTC_2D_8X8_SRGB
    MTL::PixelFormatASTC_8x5_sRGB,         // ASTC_2D_8X5_SRGB
    MTL::PixelFormatASTC_5x4_sRGB,         // ASTC_2D_5X4_SRGB
    MTL::PixelFormatASTC_5x5_LDR,          // ASTC_2D_5X5_UNORM
    MTL::PixelFormatASTC_5x5_sRGB,         // ASTC_2D_5X5_SRGB
    MTL::PixelFormatASTC_10x8_LDR,         // ASTC_2D_10X8_UNORM
    MTL::PixelFormatASTC_10x8_sRGB,        // ASTC_2D_10X8_SRGB
    MTL::PixelFormatASTC_6x6_LDR,          // ASTC_2D_6X6_UNORM
    MTL::PixelFormatASTC_6x6_sRGB,         // ASTC_2D_6X6_SRGB
    MTL::PixelFormatASTC_10x6_LDR,         // ASTC_2D_10X6_UNORM
    MTL::PixelFormatASTC_10x6_sRGB,        // ASTC_2D_10X6_SRGB
    MTL::PixelFormatASTC_10x5_LDR,         // ASTC_2D_10X5_UNORM
    MTL::PixelFormatASTC_10x5_sRGB,        // ASTC_2D_10X5_SRGB
    MTL::PixelFormatASTC_10x10_LDR,        // ASTC_2D_10X10_UNORM
    MTL::PixelFormatASTC_10x10_sRGB,       // ASTC_2D_10X10_SRGB
    MTL::PixelFormatASTC_12x10_LDR,        // ASTC_2D_12X10_UNORM
    MTL::PixelFormatASTC_12x10_sRGB,       // ASTC_2D_12X10_SRGB
    MTL::PixelFormatASTC_12x12_LDR,        // ASTC_2D_12X12_UNORM
    MTL::PixelFormatASTC_12x12_sRGB,       // ASTC_2D_12X12_SRGB
    MTL::PixelFormatASTC_8x6_LDR,          // ASTC_2D_8X6_UNORM
    MTL::PixelFormatASTC_8x6_sRGB,         // ASTC_2D_8X6_SRGB
    MTL::PixelFormatASTC_6x5_LDR,          // ASTC_2D_6X5_UNORM
    MTL::PixelFormatASTC_6x5_sRGB,         // ASTC_2D_6X5_SRGB
    MTL::PixelFormatInvalid,               // E5B9G9R9_FLOAT
    MTL::PixelFormatDepth32Float,          // D32_FLOAT
    MTL::PixelFormatDepth16Unorm,          // D16_UNORM
    MTL::PixelFormatInvalid,               // X8_D24_UNORM
    MTL::PixelFormatStencil8,              // S8_UINT
    MTL::PixelFormatDepth24Unorm_Stencil8, // D24_UNORM_S8_UINT
    MTL::PixelFormatInvalid,               // S8_UINT_D24_UNORM
    MTL::PixelFormatDepth32Float_Stencil8, // D32_FLOAT_S8_UINT
};

inline MTL::PixelFormat GetPixelFormat(VideoCore::Surface::PixelFormat pixel_format) {
    ASSERT(static_cast<size_t>(pixel_format) < FORMAT_TABLE.size());

    return FORMAT_TABLE[static_cast<size_t>(pixel_format)];
}

} // namespace Metal::MaxwellToMTL
