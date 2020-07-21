// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/morton.h"
#include "video_core/surface.h"
#include "video_core/textures/decoders.h"

namespace VideoCore {

using Surface::GetBytesPerPixel;
using Surface::PixelFormat;

using MortonCopyFn = void (*)(u32, u32, u32, u32, u32, u32, u8*, u8*);
using ConversionArray = std::array<MortonCopyFn, Surface::MaxPixelFormat>;

template <bool morton_to_linear, PixelFormat format>
static void MortonCopy(u32 stride, u32 block_height, u32 height, u32 block_depth, u32 depth,
                       u32 tile_width_spacing, u8* buffer, u8* addr) {
    constexpr u32 bytes_per_pixel = GetBytesPerPixel(format);

    // With the BCn formats (DXT and DXN), each 4x4 tile is swizzled instead of just individual
    // pixel values.
    constexpr u32 tile_size_x{GetDefaultBlockWidth(format)};
    constexpr u32 tile_size_y{GetDefaultBlockHeight(format)};

    if constexpr (morton_to_linear) {
        Tegra::Texture::UnswizzleTexture(buffer, addr, tile_size_x, tile_size_y, bytes_per_pixel,
                                         stride, height, depth, block_height, block_depth,
                                         tile_width_spacing);
    } else {
        Tegra::Texture::CopySwizzledData((stride + tile_size_x - 1) / tile_size_x,
                                         (height + tile_size_y - 1) / tile_size_y, depth,
                                         bytes_per_pixel, bytes_per_pixel, addr, buffer, false,
                                         block_height, block_depth, tile_width_spacing);
    }
}

static constexpr ConversionArray morton_to_linear_fns = {
    MortonCopy<true, PixelFormat::A8B8G8R8_UNORM>,
    MortonCopy<true, PixelFormat::A8B8G8R8_SNORM>,
    MortonCopy<true, PixelFormat::A8B8G8R8_SINT>,
    MortonCopy<true, PixelFormat::A8B8G8R8_UINT>,
    MortonCopy<true, PixelFormat::R5G6B5_UNORM>,
    MortonCopy<true, PixelFormat::B5G6R5_UNORM>,
    MortonCopy<true, PixelFormat::A1R5G5B5_UNORM>,
    MortonCopy<true, PixelFormat::A2B10G10R10_UNORM>,
    MortonCopy<true, PixelFormat::A2B10G10R10_UINT>,
    MortonCopy<true, PixelFormat::A1B5G5R5_UNORM>,
    MortonCopy<true, PixelFormat::R8_UNORM>,
    MortonCopy<true, PixelFormat::R8_SNORM>,
    MortonCopy<true, PixelFormat::R8_SINT>,
    MortonCopy<true, PixelFormat::R8_UINT>,
    MortonCopy<true, PixelFormat::R16G16B16A16_FLOAT>,
    MortonCopy<true, PixelFormat::R16G16B16A16_UNORM>,
    MortonCopy<true, PixelFormat::R16G16B16A16_SNORM>,
    MortonCopy<true, PixelFormat::R16G16B16A16_SINT>,
    MortonCopy<true, PixelFormat::R16G16B16A16_UINT>,
    MortonCopy<true, PixelFormat::B10G11R11_FLOAT>,
    MortonCopy<true, PixelFormat::R32G32B32A32_UINT>,
    MortonCopy<true, PixelFormat::BC1_RGBA_UNORM>,
    MortonCopy<true, PixelFormat::BC2_UNORM>,
    MortonCopy<true, PixelFormat::BC3_UNORM>,
    MortonCopy<true, PixelFormat::BC4_UNORM>,
    MortonCopy<true, PixelFormat::BC4_SNORM>,
    MortonCopy<true, PixelFormat::BC5_UNORM>,
    MortonCopy<true, PixelFormat::BC5_SNORM>,
    MortonCopy<true, PixelFormat::BC7_UNORM>,
    MortonCopy<true, PixelFormat::BC6H_UFLOAT>,
    MortonCopy<true, PixelFormat::BC6H_SFLOAT>,
    MortonCopy<true, PixelFormat::ASTC_2D_4X4_UNORM>,
    MortonCopy<true, PixelFormat::B8G8R8A8_UNORM>,
    MortonCopy<true, PixelFormat::R32G32B32A32_FLOAT>,
    MortonCopy<true, PixelFormat::R32G32B32A32_SINT>,
    MortonCopy<true, PixelFormat::R32G32_FLOAT>,
    MortonCopy<true, PixelFormat::R32G32_SINT>,
    MortonCopy<true, PixelFormat::R32_FLOAT>,
    MortonCopy<true, PixelFormat::R16_FLOAT>,
    MortonCopy<true, PixelFormat::R16_UNORM>,
    MortonCopy<true, PixelFormat::R16_SNORM>,
    MortonCopy<true, PixelFormat::R16_UINT>,
    MortonCopy<true, PixelFormat::R16_SINT>,
    MortonCopy<true, PixelFormat::R16G16_UNORM>,
    MortonCopy<true, PixelFormat::R16G16_FLOAT>,
    MortonCopy<true, PixelFormat::R16G16_UINT>,
    MortonCopy<true, PixelFormat::R16G16_SINT>,
    MortonCopy<true, PixelFormat::R16G16_SNORM>,
    MortonCopy<true, PixelFormat::R32G32B32_FLOAT>,
    MortonCopy<true, PixelFormat::A8B8G8R8_SRGB>,
    MortonCopy<true, PixelFormat::R8G8_UNORM>,
    MortonCopy<true, PixelFormat::R8G8_SNORM>,
    MortonCopy<true, PixelFormat::R8G8_SINT>,
    MortonCopy<true, PixelFormat::R8G8_UINT>,
    MortonCopy<true, PixelFormat::R32G32_UINT>,
    MortonCopy<true, PixelFormat::R16G16B16X16_FLOAT>,
    MortonCopy<true, PixelFormat::R32_UINT>,
    MortonCopy<true, PixelFormat::R32_SINT>,
    MortonCopy<true, PixelFormat::ASTC_2D_8X8_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_8X5_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_5X4_UNORM>,
    MortonCopy<true, PixelFormat::B8G8R8A8_SRGB>,
    MortonCopy<true, PixelFormat::BC1_RGBA_SRGB>,
    MortonCopy<true, PixelFormat::BC2_SRGB>,
    MortonCopy<true, PixelFormat::BC3_SRGB>,
    MortonCopy<true, PixelFormat::BC7_SRGB>,
    MortonCopy<true, PixelFormat::A4B4G4R4_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_4X4_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_8X8_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_8X5_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_5X4_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_5X5_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_5X5_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_10X8_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_10X8_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_6X6_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_6X6_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_10X10_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_10X10_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_12X12_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_12X12_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_8X6_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_8X6_SRGB>,
    MortonCopy<true, PixelFormat::ASTC_2D_6X5_UNORM>,
    MortonCopy<true, PixelFormat::ASTC_2D_6X5_SRGB>,
    MortonCopy<true, PixelFormat::E5B9G9R9_FLOAT>,
    MortonCopy<true, PixelFormat::D32_FLOAT>,
    MortonCopy<true, PixelFormat::D16_UNORM>,
    MortonCopy<true, PixelFormat::D24_UNORM_S8_UINT>,
    MortonCopy<true, PixelFormat::S8_UINT_D24_UNORM>,
    MortonCopy<true, PixelFormat::D32_FLOAT_S8_UINT>,
};

static constexpr ConversionArray linear_to_morton_fns = {
    MortonCopy<false, PixelFormat::A8B8G8R8_UNORM>,
    MortonCopy<false, PixelFormat::A8B8G8R8_SNORM>,
    MortonCopy<false, PixelFormat::A8B8G8R8_SINT>,
    MortonCopy<false, PixelFormat::A8B8G8R8_UINT>,
    MortonCopy<false, PixelFormat::R5G6B5_UNORM>,
    MortonCopy<false, PixelFormat::B5G6R5_UNORM>,
    MortonCopy<false, PixelFormat::A1R5G5B5_UNORM>,
    MortonCopy<false, PixelFormat::A2B10G10R10_UNORM>,
    MortonCopy<false, PixelFormat::A2B10G10R10_UINT>,
    MortonCopy<false, PixelFormat::A1B5G5R5_UNORM>,
    MortonCopy<false, PixelFormat::R8_UNORM>,
    MortonCopy<false, PixelFormat::R8_SNORM>,
    MortonCopy<false, PixelFormat::R8_SINT>,
    MortonCopy<false, PixelFormat::R8_UINT>,
    MortonCopy<false, PixelFormat::R16G16B16A16_FLOAT>,
    MortonCopy<false, PixelFormat::R16G16B16A16_SNORM>,
    MortonCopy<false, PixelFormat::R16G16B16A16_SINT>,
    MortonCopy<false, PixelFormat::R16G16B16A16_UNORM>,
    MortonCopy<false, PixelFormat::R16G16B16A16_UINT>,
    MortonCopy<false, PixelFormat::B10G11R11_FLOAT>,
    MortonCopy<false, PixelFormat::R32G32B32A32_UINT>,
    MortonCopy<false, PixelFormat::BC1_RGBA_UNORM>,
    MortonCopy<false, PixelFormat::BC2_UNORM>,
    MortonCopy<false, PixelFormat::BC3_UNORM>,
    MortonCopy<false, PixelFormat::BC4_UNORM>,
    MortonCopy<false, PixelFormat::BC4_SNORM>,
    MortonCopy<false, PixelFormat::BC5_UNORM>,
    MortonCopy<false, PixelFormat::BC5_SNORM>,
    MortonCopy<false, PixelFormat::BC7_UNORM>,
    MortonCopy<false, PixelFormat::BC6H_UFLOAT>,
    MortonCopy<false, PixelFormat::BC6H_SFLOAT>,
    // TODO(Subv): Swizzling ASTC formats are not supported
    nullptr,
    MortonCopy<false, PixelFormat::B8G8R8A8_UNORM>,
    MortonCopy<false, PixelFormat::R32G32B32A32_FLOAT>,
    MortonCopy<false, PixelFormat::R32G32B32A32_SINT>,
    MortonCopy<false, PixelFormat::R32G32_FLOAT>,
    MortonCopy<false, PixelFormat::R32G32_SINT>,
    MortonCopy<false, PixelFormat::R32_FLOAT>,
    MortonCopy<false, PixelFormat::R16_FLOAT>,
    MortonCopy<false, PixelFormat::R16_UNORM>,
    MortonCopy<false, PixelFormat::R16_SNORM>,
    MortonCopy<false, PixelFormat::R16_UINT>,
    MortonCopy<false, PixelFormat::R16_SINT>,
    MortonCopy<false, PixelFormat::R16G16_UNORM>,
    MortonCopy<false, PixelFormat::R16G16_FLOAT>,
    MortonCopy<false, PixelFormat::R16G16_UINT>,
    MortonCopy<false, PixelFormat::R16G16_SINT>,
    MortonCopy<false, PixelFormat::R16G16_SNORM>,
    MortonCopy<false, PixelFormat::R32G32B32_FLOAT>,
    MortonCopy<false, PixelFormat::A8B8G8R8_SRGB>,
    MortonCopy<false, PixelFormat::R8G8_UNORM>,
    MortonCopy<false, PixelFormat::R8G8_SNORM>,
    MortonCopy<false, PixelFormat::R8G8_SINT>,
    MortonCopy<false, PixelFormat::R8G8_UINT>,
    MortonCopy<false, PixelFormat::R32G32_UINT>,
    MortonCopy<false, PixelFormat::R16G16B16X16_FLOAT>,
    MortonCopy<false, PixelFormat::R32_UINT>,
    MortonCopy<false, PixelFormat::R32_SINT>,
    nullptr,
    nullptr,
    nullptr,
    MortonCopy<false, PixelFormat::B8G8R8A8_SRGB>,
    MortonCopy<false, PixelFormat::BC1_RGBA_SRGB>,
    MortonCopy<false, PixelFormat::BC2_SRGB>,
    MortonCopy<false, PixelFormat::BC3_SRGB>,
    MortonCopy<false, PixelFormat::BC7_SRGB>,
    MortonCopy<false, PixelFormat::A4B4G4R4_UNORM>,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    MortonCopy<false, PixelFormat::E5B9G9R9_FLOAT>,
    MortonCopy<false, PixelFormat::D32_FLOAT>,
    MortonCopy<false, PixelFormat::D16_UNORM>,
    MortonCopy<false, PixelFormat::D24_UNORM_S8_UINT>,
    MortonCopy<false, PixelFormat::S8_UINT_D24_UNORM>,
    MortonCopy<false, PixelFormat::D32_FLOAT_S8_UINT>,
};

static MortonCopyFn GetSwizzleFunction(MortonSwizzleMode mode, Surface::PixelFormat format) {
    switch (mode) {
    case MortonSwizzleMode::MortonToLinear:
        return morton_to_linear_fns[static_cast<std::size_t>(format)];
    case MortonSwizzleMode::LinearToMorton:
        return linear_to_morton_fns[static_cast<std::size_t>(format)];
    }
    UNREACHABLE();
    return morton_to_linear_fns[static_cast<std::size_t>(format)];
}

void MortonSwizzle(MortonSwizzleMode mode, Surface::PixelFormat format, u32 stride,
                   u32 block_height, u32 height, u32 block_depth, u32 depth, u32 tile_width_spacing,
                   u8* buffer, u8* addr) {
    GetSwizzleFunction(mode, format)(stride, block_height, height, block_depth, depth,
                                     tile_width_spacing, buffer, addr);
}

} // namespace VideoCore
