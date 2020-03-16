// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/surface.h"

namespace VideoCore::Surface {

SurfaceTarget SurfaceTargetFromTextureType(Tegra::Texture::TextureType texture_type) {
    switch (texture_type) {
    case Tegra::Texture::TextureType::Texture1D:
        return SurfaceTarget::Texture1D;
    case Tegra::Texture::TextureType::Texture1DBuffer:
        return SurfaceTarget::TextureBuffer;
    case Tegra::Texture::TextureType::Texture2D:
    case Tegra::Texture::TextureType::Texture2DNoMipmap:
        return SurfaceTarget::Texture2D;
    case Tegra::Texture::TextureType::Texture3D:
        return SurfaceTarget::Texture3D;
    case Tegra::Texture::TextureType::TextureCubemap:
        return SurfaceTarget::TextureCubemap;
    case Tegra::Texture::TextureType::TextureCubeArray:
        return SurfaceTarget::TextureCubeArray;
    case Tegra::Texture::TextureType::Texture1DArray:
        return SurfaceTarget::Texture1DArray;
    case Tegra::Texture::TextureType::Texture2DArray:
        return SurfaceTarget::Texture2DArray;
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented texture_type={}", static_cast<u32>(texture_type));
        UNREACHABLE();
        return SurfaceTarget::Texture2D;
    }
}

bool SurfaceTargetIsLayered(SurfaceTarget target) {
    switch (target) {
    case SurfaceTarget::Texture1D:
    case SurfaceTarget::TextureBuffer:
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::Texture3D:
        return false;
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubemap:
    case SurfaceTarget::TextureCubeArray:
        return true;
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented surface_target={}", static_cast<u32>(target));
        UNREACHABLE();
        return false;
    }
}

bool SurfaceTargetIsArray(SurfaceTarget target) {
    switch (target) {
    case SurfaceTarget::Texture1D:
    case SurfaceTarget::TextureBuffer:
    case SurfaceTarget::Texture2D:
    case SurfaceTarget::Texture3D:
    case SurfaceTarget::TextureCubemap:
        return false;
    case SurfaceTarget::Texture1DArray:
    case SurfaceTarget::Texture2DArray:
    case SurfaceTarget::TextureCubeArray:
        return true;
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented surface_target={}", static_cast<u32>(target));
        UNREACHABLE();
        return false;
    }
}

PixelFormat PixelFormatFromDepthFormat(Tegra::DepthFormat format) {
    switch (format) {
    case Tegra::DepthFormat::S8_Z24_UNORM:
        return PixelFormat::S8Z24;
    case Tegra::DepthFormat::Z24_S8_UNORM:
        return PixelFormat::Z24S8;
    case Tegra::DepthFormat::Z32_FLOAT:
        return PixelFormat::Z32F;
    case Tegra::DepthFormat::Z16_UNORM:
        return PixelFormat::Z16;
    case Tegra::DepthFormat::Z32_S8_X24_FLOAT:
        return PixelFormat::Z32FS8;
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented format={}", static_cast<u32>(format));
        UNREACHABLE();
        return PixelFormat::S8Z24;
    }
}

PixelFormat PixelFormatFromRenderTargetFormat(Tegra::RenderTargetFormat format) {
    switch (format) {
    case Tegra::RenderTargetFormat::RGBA8_SRGB:
        return PixelFormat::RGBA8_SRGB;
    case Tegra::RenderTargetFormat::RGBA8_UNORM:
        return PixelFormat::ABGR8U;
    case Tegra::RenderTargetFormat::RGBA8_SNORM:
        return PixelFormat::ABGR8S;
    case Tegra::RenderTargetFormat::RGBA8_UINT:
        return PixelFormat::ABGR8UI;
    case Tegra::RenderTargetFormat::BGRA8_SRGB:
        return PixelFormat::BGRA8_SRGB;
    case Tegra::RenderTargetFormat::BGRA8_UNORM:
        return PixelFormat::BGRA8;
    case Tegra::RenderTargetFormat::RGB10_A2_UNORM:
        return PixelFormat::A2B10G10R10U;
    case Tegra::RenderTargetFormat::RGBA16_FLOAT:
        return PixelFormat::RGBA16F;
    case Tegra::RenderTargetFormat::RGBA16_UNORM:
        return PixelFormat::RGBA16U;
    case Tegra::RenderTargetFormat::RGBA16_SNORM:
        return PixelFormat::RGBA16S;
    case Tegra::RenderTargetFormat::RGBA16_UINT:
        return PixelFormat::RGBA16UI;
    case Tegra::RenderTargetFormat::RGBA32_FLOAT:
        return PixelFormat::RGBA32F;
    case Tegra::RenderTargetFormat::RG32_FLOAT:
        return PixelFormat::RG32F;
    case Tegra::RenderTargetFormat::R11G11B10_FLOAT:
        return PixelFormat::R11FG11FB10F;
    case Tegra::RenderTargetFormat::B5G6R5_UNORM:
        return PixelFormat::B5G6R5U;
    case Tegra::RenderTargetFormat::BGR5A1_UNORM:
        return PixelFormat::A1B5G5R5U;
    case Tegra::RenderTargetFormat::RGBA32_UINT:
        return PixelFormat::RGBA32UI;
    case Tegra::RenderTargetFormat::R8_UNORM:
        return PixelFormat::R8U;
    case Tegra::RenderTargetFormat::R8_UINT:
        return PixelFormat::R8UI;
    case Tegra::RenderTargetFormat::RG16_FLOAT:
        return PixelFormat::RG16F;
    case Tegra::RenderTargetFormat::RG16_UINT:
        return PixelFormat::RG16UI;
    case Tegra::RenderTargetFormat::RG16_SINT:
        return PixelFormat::RG16I;
    case Tegra::RenderTargetFormat::RG16_UNORM:
        return PixelFormat::RG16;
    case Tegra::RenderTargetFormat::RG16_SNORM:
        return PixelFormat::RG16S;
    case Tegra::RenderTargetFormat::RG8_UNORM:
        return PixelFormat::RG8U;
    case Tegra::RenderTargetFormat::RG8_SNORM:
        return PixelFormat::RG8S;
    case Tegra::RenderTargetFormat::R16_FLOAT:
        return PixelFormat::R16F;
    case Tegra::RenderTargetFormat::R16_UNORM:
        return PixelFormat::R16U;
    case Tegra::RenderTargetFormat::R16_SNORM:
        return PixelFormat::R16S;
    case Tegra::RenderTargetFormat::R16_UINT:
        return PixelFormat::R16UI;
    case Tegra::RenderTargetFormat::R16_SINT:
        return PixelFormat::R16I;
    case Tegra::RenderTargetFormat::R32_FLOAT:
        return PixelFormat::R32F;
    case Tegra::RenderTargetFormat::R32_SINT:
        return PixelFormat::R32I;
    case Tegra::RenderTargetFormat::R32_UINT:
        return PixelFormat::R32UI;
    case Tegra::RenderTargetFormat::RG32_UINT:
        return PixelFormat::RG32UI;
    case Tegra::RenderTargetFormat::RGBX16_FLOAT:
        return PixelFormat::RGBX16F;
    default:
        LOG_CRITICAL(HW_GPU, "Unimplemented format={}", static_cast<u32>(format));
        UNREACHABLE();
        return PixelFormat::RGBA8_SRGB;
    }
}

PixelFormat PixelFormatFromGPUPixelFormat(Tegra::FramebufferConfig::PixelFormat format) {
    switch (format) {
    case Tegra::FramebufferConfig::PixelFormat::ABGR8:
        return PixelFormat::ABGR8U;
    case Tegra::FramebufferConfig::PixelFormat::RGB565:
        return PixelFormat::B5G6R5U;
    case Tegra::FramebufferConfig::PixelFormat::BGRA8:
        return PixelFormat::BGRA8;
    default:
        UNIMPLEMENTED_MSG("Unimplemented format={}", static_cast<u32>(format));
        return PixelFormat::ABGR8U;
    }
}

SurfaceType GetFormatType(PixelFormat pixel_format) {
    if (static_cast<std::size_t>(pixel_format) <
        static_cast<std::size_t>(PixelFormat::MaxColorFormat)) {
        return SurfaceType::ColorTexture;
    }

    if (static_cast<std::size_t>(pixel_format) <
        static_cast<std::size_t>(PixelFormat::MaxDepthFormat)) {
        return SurfaceType::Depth;
    }

    if (static_cast<std::size_t>(pixel_format) <
        static_cast<std::size_t>(PixelFormat::MaxDepthStencilFormat)) {
        return SurfaceType::DepthStencil;
    }

    // TODO(Subv): Implement the other formats
    ASSERT(false);

    return SurfaceType::Invalid;
}

bool IsPixelFormatASTC(PixelFormat format) {
    switch (format) {
    case PixelFormat::ASTC_2D_4X4:
    case PixelFormat::ASTC_2D_5X4:
    case PixelFormat::ASTC_2D_5X5:
    case PixelFormat::ASTC_2D_8X8:
    case PixelFormat::ASTC_2D_8X5:
    case PixelFormat::ASTC_2D_4X4_SRGB:
    case PixelFormat::ASTC_2D_5X4_SRGB:
    case PixelFormat::ASTC_2D_5X5_SRGB:
    case PixelFormat::ASTC_2D_8X8_SRGB:
    case PixelFormat::ASTC_2D_8X5_SRGB:
    case PixelFormat::ASTC_2D_10X8:
    case PixelFormat::ASTC_2D_10X8_SRGB:
    case PixelFormat::ASTC_2D_6X6:
    case PixelFormat::ASTC_2D_6X6_SRGB:
    case PixelFormat::ASTC_2D_10X10:
    case PixelFormat::ASTC_2D_10X10_SRGB:
    case PixelFormat::ASTC_2D_12X12:
    case PixelFormat::ASTC_2D_12X12_SRGB:
    case PixelFormat::ASTC_2D_8X6:
    case PixelFormat::ASTC_2D_8X6_SRGB:
    case PixelFormat::ASTC_2D_6X5:
    case PixelFormat::ASTC_2D_6X5_SRGB:
        return true;
    default:
        return false;
    }
}

bool IsPixelFormatSRGB(PixelFormat format) {
    switch (format) {
    case PixelFormat::RGBA8_SRGB:
    case PixelFormat::BGRA8_SRGB:
    case PixelFormat::DXT1_SRGB:
    case PixelFormat::DXT23_SRGB:
    case PixelFormat::DXT45_SRGB:
    case PixelFormat::BC7U_SRGB:
    case PixelFormat::ASTC_2D_4X4_SRGB:
    case PixelFormat::ASTC_2D_8X8_SRGB:
    case PixelFormat::ASTC_2D_8X5_SRGB:
    case PixelFormat::ASTC_2D_5X4_SRGB:
    case PixelFormat::ASTC_2D_5X5_SRGB:
    case PixelFormat::ASTC_2D_10X8_SRGB:
    case PixelFormat::ASTC_2D_6X6_SRGB:
    case PixelFormat::ASTC_2D_10X10_SRGB:
    case PixelFormat::ASTC_2D_12X12_SRGB:
    case PixelFormat::ASTC_2D_8X6_SRGB:
    case PixelFormat::ASTC_2D_6X5_SRGB:
        return true;
    default:
        return false;
    }
}

std::pair<u32, u32> GetASTCBlockSize(PixelFormat format) {
    return {GetDefaultBlockWidth(format), GetDefaultBlockHeight(format)};
}

bool IsFormatBCn(PixelFormat format) {
    switch (format) {
    case PixelFormat::DXT1:
    case PixelFormat::DXT23:
    case PixelFormat::DXT45:
    case PixelFormat::DXN1:
    case PixelFormat::DXN2SNORM:
    case PixelFormat::DXN2UNORM:
    case PixelFormat::BC7U:
    case PixelFormat::BC6H_UF16:
    case PixelFormat::BC6H_SF16:
    case PixelFormat::DXT1_SRGB:
    case PixelFormat::DXT23_SRGB:
    case PixelFormat::DXT45_SRGB:
    case PixelFormat::BC7U_SRGB:
        return true;
    default:
        return false;
    }
}

} // namespace VideoCore::Surface
