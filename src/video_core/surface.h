// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <climits>
#include <utility>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/gpu.h"
#include "video_core/textures/texture.h"

namespace VideoCore::Surface {

#define PIXEL_FORMAT_LIST \
    PIXEL_FORMAT_ELEM(A8B8G8R8_UNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(A8B8G8R8_SNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(A8B8G8R8_SINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(A8B8G8R8_UINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R5G6B5_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(B5G6R5_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(A1R5G5B5_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(A2B10G10R10_UNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(A2B10G10R10_UINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(A2R10G10B10_UNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(A1B5G5R5_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(A5B5G5R1_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R8_UNORM, 1, 1, 8) \
    PIXEL_FORMAT_ELEM(R8_SNORM, 1, 1, 8) \
    PIXEL_FORMAT_ELEM(R8_SINT, 1, 1, 8) \
    PIXEL_FORMAT_ELEM(R8_UINT, 1, 1, 8) \
    PIXEL_FORMAT_ELEM(R16G16B16A16_FLOAT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R16G16B16A16_UNORM, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R16G16B16A16_SNORM, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R16G16B16A16_SINT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R16G16B16A16_UINT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(B10G11R11_FLOAT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R32G32B32A32_UINT, 1, 1, 128) \
    PIXEL_FORMAT_ELEM(BC1_RGBA_UNORM, 4, 4, 64) \
    PIXEL_FORMAT_ELEM(BC2_UNORM, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC3_UNORM, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC4_UNORM, 4, 4, 64) \
    PIXEL_FORMAT_ELEM(BC4_SNORM, 4, 4, 64) \
    PIXEL_FORMAT_ELEM(BC5_UNORM, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC5_SNORM, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC7_UNORM, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC6H_UFLOAT, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC6H_SFLOAT, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_4X4_UNORM, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(B8G8R8A8_UNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R32G32B32A32_FLOAT, 1, 1, 128) \
    PIXEL_FORMAT_ELEM(R32G32B32A32_SINT, 1, 1, 128) \
    PIXEL_FORMAT_ELEM(R32G32_FLOAT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R32G32_SINT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R32_FLOAT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R16_FLOAT, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R16_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R16_SNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R16_UINT, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R16_SINT, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R16G16_UNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R16G16_FLOAT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R16G16_UINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R16G16_SINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R16G16_SNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R32G32B32_FLOAT, 1, 1, 96) \
    PIXEL_FORMAT_ELEM(A8B8G8R8_SRGB, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R8G8_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R8G8_SNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R8G8_SINT, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R8G8_UINT, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(R32G32_UINT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R16G16B16X16_FLOAT, 1, 1, 64) \
    PIXEL_FORMAT_ELEM(R32_UINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(R32_SINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(ASTC_2D_8X8_UNORM, 8, 8, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_8X5_UNORM, 8, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_5X4_UNORM, 5, 4, 128) \
    PIXEL_FORMAT_ELEM(B8G8R8A8_SRGB, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(BC1_RGBA_SRGB, 4, 4, 64) \
    PIXEL_FORMAT_ELEM(BC2_SRGB, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC3_SRGB, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(BC7_SRGB, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(A4B4G4R4_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(G4R4_UNORM, 1, 1, 8) \
    PIXEL_FORMAT_ELEM(ASTC_2D_4X4_SRGB, 4, 4, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_8X8_SRGB, 8, 8, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_8X5_SRGB, 8, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_5X4_SRGB, 5, 4, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_5X5_UNORM, 5, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_5X5_SRGB, 5, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X8_UNORM, 10, 8, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X8_SRGB, 10, 8, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_6X6_UNORM, 6, 6, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_6X6_SRGB, 6, 6, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X6_UNORM, 10, 6, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X6_SRGB, 10, 6, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X5_UNORM, 10, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X5_SRGB, 10, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X10_UNORM, 10, 10, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_10X10_SRGB, 10, 10, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_12X10_UNORM, 12, 10, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_12X10_SRGB, 12, 10, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_12X12_UNORM, 12, 12, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_12X12_SRGB, 12, 12, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_8X6_UNORM, 8, 6, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_8X6_SRGB, 8, 6, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_6X5_UNORM, 6, 5, 128) \
    PIXEL_FORMAT_ELEM(ASTC_2D_6X5_SRGB, 6, 5, 128) \
    PIXEL_FORMAT_ELEM(E5B9G9R9_FLOAT, 1, 1, 32) \
    /* Depth formats */ \
    PIXEL_FORMAT_ELEM(D32_FLOAT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(D16_UNORM, 1, 1, 16) \
    PIXEL_FORMAT_ELEM(X8_D24_UNORM, 1, 1, 32) \
    /* Stencil formats */ \
    PIXEL_FORMAT_ELEM(S8_UINT, 1, 1, 8) \
    /* DepthStencil formats */ \
    PIXEL_FORMAT_ELEM(D24_UNORM_S8_UINT, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(S8_UINT_D24_UNORM, 1, 1, 32) \
    PIXEL_FORMAT_ELEM(D32_FLOAT_S8_UINT, 1, 1, 64)

enum class PixelFormat {
#define PIXEL_FORMAT_ELEM(name, ...) name,
    PIXEL_FORMAT_LIST
#undef PIXEL_FORMAT_ELEM
    MaxColorFormat = D32_FLOAT,
    MaxDepthFormat = S8_UINT,
    MaxStencilFormat = D24_UNORM_S8_UINT,
    MaxDepthStencilFormat = u8(D32_FLOAT_S8_UINT) + 1,
    Max = MaxDepthStencilFormat,
    Invalid = 255,
};
constexpr std::size_t MaxPixelFormat = std::size_t(PixelFormat::Max);

enum class SurfaceType {
    ColorTexture = 0,
    Depth = 1,
    Stencil = 2,
    DepthStencil = 3,
    Invalid = 4,
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

constexpr u32 DefaultBlockWidth(PixelFormat format) noexcept {
    switch (format) {
#define PIXEL_FORMAT_ELEM(name, width, height, bits) case PixelFormat::name: return width;
    PIXEL_FORMAT_LIST
#undef PIXEL_FORMAT_ELEM
    default: UNREACHABLE();
    }
}

constexpr u32 DefaultBlockHeight(PixelFormat format) noexcept {
    switch (format) {
#define PIXEL_FORMAT_ELEM(name, width, height, bits) case PixelFormat::name: return height;
    PIXEL_FORMAT_LIST
#undef PIXEL_FORMAT_ELEM
    default: UNREACHABLE();
    }
}

constexpr u32 BitsPerBlock(PixelFormat format) noexcept {
    switch (format) {
#define PIXEL_FORMAT_ELEM(name, width, height, bits) case PixelFormat::name: return bits;
    PIXEL_FORMAT_LIST
#undef PIXEL_FORMAT_ELEM
    default: UNREACHABLE();
    }
}

#undef PIXEL_FORMAT_LIST

/// Returns the sizer in bytes of the specified pixel format
constexpr u32 BytesPerBlock(PixelFormat pixel_format) {
    return BitsPerBlock(pixel_format) / CHAR_BIT;
}

SurfaceTarget SurfaceTargetFromTextureType(Tegra::Texture::TextureType texture_type);
bool SurfaceTargetIsLayered(SurfaceTarget target);
bool SurfaceTargetIsArray(SurfaceTarget target);
PixelFormat PixelFormatFromDepthFormat(Tegra::DepthFormat format);
PixelFormat PixelFormatFromRenderTargetFormat(Tegra::RenderTargetFormat format);
PixelFormat PixelFormatFromGPUPixelFormat(Service::android::PixelFormat format);
SurfaceType GetFormatType(PixelFormat pixel_format);
bool HasAlpha(PixelFormat pixel_format);
bool IsPixelFormatASTC(PixelFormat format);
bool IsPixelFormatBCn(PixelFormat format);
bool IsPixelFormatSRGB(PixelFormat format);
bool IsPixelFormatInteger(PixelFormat format);
bool IsPixelFormatSignedInteger(PixelFormat format);
size_t PixelComponentSizeBitsInteger(PixelFormat format);
std::pair<u32, u32> GetASTCBlockSize(PixelFormat format);
u64 TranscodedAstcSize(u64 base_size, PixelFormat format);

} // namespace VideoCore::Surface
