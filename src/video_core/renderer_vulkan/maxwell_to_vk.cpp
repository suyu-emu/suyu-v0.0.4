// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/surface.h"

namespace Vulkan::MaxwellToVK {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

namespace Sampler {

vk::Filter Filter(Tegra::Texture::TextureFilter filter) {
    switch (filter) {
    case Tegra::Texture::TextureFilter::Linear:
        return vk::Filter::eLinear;
    case Tegra::Texture::TextureFilter::Nearest:
        return vk::Filter::eNearest;
    }
    UNIMPLEMENTED_MSG("Unimplemented sampler filter={}", static_cast<u32>(filter));
    return {};
}

vk::SamplerMipmapMode MipmapMode(Tegra::Texture::TextureMipmapFilter mipmap_filter) {
    switch (mipmap_filter) {
    case Tegra::Texture::TextureMipmapFilter::None:
        // TODO(Rodrigo): None seems to be mapped to OpenGL's mag and min filters without mipmapping
        // (e.g. GL_NEAREST and GL_LINEAR). Vulkan doesn't have such a thing, find out if we have to
        // use an image view with a single mipmap level to emulate this.
        return vk::SamplerMipmapMode::eLinear;
    case Tegra::Texture::TextureMipmapFilter::Linear:
        return vk::SamplerMipmapMode::eLinear;
    case Tegra::Texture::TextureMipmapFilter::Nearest:
        return vk::SamplerMipmapMode::eNearest;
    }
    UNIMPLEMENTED_MSG("Unimplemented sampler mipmap mode={}", static_cast<u32>(mipmap_filter));
    return {};
}

vk::SamplerAddressMode WrapMode(const VKDevice& device, Tegra::Texture::WrapMode wrap_mode,
                                Tegra::Texture::TextureFilter filter) {
    switch (wrap_mode) {
    case Tegra::Texture::WrapMode::Wrap:
        return vk::SamplerAddressMode::eRepeat;
    case Tegra::Texture::WrapMode::Mirror:
        return vk::SamplerAddressMode::eMirroredRepeat;
    case Tegra::Texture::WrapMode::ClampToEdge:
        return vk::SamplerAddressMode::eClampToEdge;
    case Tegra::Texture::WrapMode::Border:
        return vk::SamplerAddressMode::eClampToBorder;
    case Tegra::Texture::WrapMode::Clamp:
        if (device.GetDriverID() == vk::DriverIdKHR::eNvidiaProprietary) {
            // Nvidia's Vulkan driver defaults to GL_CLAMP on invalid enumerations, we can hack this
            // by sending an invalid enumeration.
            return static_cast<vk::SamplerAddressMode>(0xcafe);
        }
        // TODO(Rodrigo): Emulate GL_CLAMP properly on other vendors
        switch (filter) {
        case Tegra::Texture::TextureFilter::Nearest:
            return vk::SamplerAddressMode::eClampToEdge;
        case Tegra::Texture::TextureFilter::Linear:
            return vk::SamplerAddressMode::eClampToBorder;
        }
        UNREACHABLE();
        return vk::SamplerAddressMode::eClampToEdge;
    case Tegra::Texture::WrapMode::MirrorOnceClampToEdge:
        return vk::SamplerAddressMode::eMirrorClampToEdge;
    case Tegra::Texture::WrapMode::MirrorOnceBorder:
        UNIMPLEMENTED();
        return vk::SamplerAddressMode::eMirrorClampToEdge;
    default:
        UNIMPLEMENTED_MSG("Unimplemented wrap mode={}", static_cast<u32>(wrap_mode));
        return {};
    }
}

vk::CompareOp DepthCompareFunction(Tegra::Texture::DepthCompareFunc depth_compare_func) {
    switch (depth_compare_func) {
    case Tegra::Texture::DepthCompareFunc::Never:
        return vk::CompareOp::eNever;
    case Tegra::Texture::DepthCompareFunc::Less:
        return vk::CompareOp::eLess;
    case Tegra::Texture::DepthCompareFunc::LessEqual:
        return vk::CompareOp::eLessOrEqual;
    case Tegra::Texture::DepthCompareFunc::Equal:
        return vk::CompareOp::eEqual;
    case Tegra::Texture::DepthCompareFunc::NotEqual:
        return vk::CompareOp::eNotEqual;
    case Tegra::Texture::DepthCompareFunc::Greater:
        return vk::CompareOp::eGreater;
    case Tegra::Texture::DepthCompareFunc::GreaterEqual:
        return vk::CompareOp::eGreaterOrEqual;
    case Tegra::Texture::DepthCompareFunc::Always:
        return vk::CompareOp::eAlways;
    }
    UNIMPLEMENTED_MSG("Unimplemented sampler depth compare function={}",
                      static_cast<u32>(depth_compare_func));
    return {};
}

} // namespace Sampler

namespace {

enum : u32 { Attachable = 1, Storage = 2 };

struct FormatTuple {
    vk::Format format; ///< Vulkan format
    int usage;         ///< Describes image format usage
} constexpr tex_format_tuples[] = {
    {vk::Format::eA8B8G8R8UnormPack32, Attachable | Storage},    // ABGR8U
    {vk::Format::eA8B8G8R8SnormPack32, Attachable | Storage},    // ABGR8S
    {vk::Format::eA8B8G8R8UintPack32, Attachable | Storage},     // ABGR8UI
    {vk::Format::eB5G6R5UnormPack16, {}},                        // B5G6R5U
    {vk::Format::eA2B10G10R10UnormPack32, Attachable | Storage}, // A2B10G10R10U
    {vk::Format::eA1R5G5B5UnormPack16, Attachable},              // A1B5G5R5U (flipped with swizzle)
    {vk::Format::eR8Unorm, Attachable | Storage},                // R8U
    {vk::Format::eR8Uint, Attachable | Storage},                 // R8UI
    {vk::Format::eR16G16B16A16Sfloat, Attachable | Storage},     // RGBA16F
    {vk::Format::eR16G16B16A16Unorm, Attachable | Storage},      // RGBA16U
    {vk::Format::eR16G16B16A16Snorm, Attachable | Storage},      // RGBA16S
    {vk::Format::eR16G16B16A16Uint, Attachable | Storage},       // RGBA16UI
    {vk::Format::eB10G11R11UfloatPack32, Attachable | Storage},  // R11FG11FB10F
    {vk::Format::eR32G32B32A32Uint, Attachable | Storage},       // RGBA32UI
    {vk::Format::eBc1RgbaUnormBlock, {}},                        // DXT1
    {vk::Format::eBc2UnormBlock, {}},                            // DXT23
    {vk::Format::eBc3UnormBlock, {}},                            // DXT45
    {vk::Format::eBc4UnormBlock, {}},                            // DXN1
    {vk::Format::eBc5UnormBlock, {}},                            // DXN2UNORM
    {vk::Format::eBc5SnormBlock, {}},                            // DXN2SNORM
    {vk::Format::eBc7UnormBlock, {}},                            // BC7U
    {vk::Format::eBc6HUfloatBlock, {}},                          // BC6H_UF16
    {vk::Format::eBc6HSfloatBlock, {}},                          // BC6H_SF16
    {vk::Format::eAstc4x4UnormBlock, {}},                        // ASTC_2D_4X4
    {vk::Format::eB8G8R8A8Unorm, {}},                            // BGRA8
    {vk::Format::eR32G32B32A32Sfloat, Attachable | Storage},     // RGBA32F
    {vk::Format::eR32G32Sfloat, Attachable | Storage},           // RG32F
    {vk::Format::eR32Sfloat, Attachable | Storage},              // R32F
    {vk::Format::eR16Sfloat, Attachable | Storage},              // R16F
    {vk::Format::eR16Unorm, Attachable | Storage},               // R16U
    {vk::Format::eUndefined, {}},                                // R16S
    {vk::Format::eUndefined, {}},                                // R16UI
    {vk::Format::eUndefined, {}},                                // R16I
    {vk::Format::eR16G16Unorm, Attachable | Storage},            // RG16
    {vk::Format::eR16G16Sfloat, Attachable | Storage},           // RG16F
    {vk::Format::eUndefined, {}},                                // RG16UI
    {vk::Format::eUndefined, {}},                                // RG16I
    {vk::Format::eR16G16Snorm, Attachable | Storage},            // RG16S
    {vk::Format::eUndefined, {}},                                // RGB32F
    {vk::Format::eR8G8B8A8Srgb, Attachable},                     // RGBA8_SRGB
    {vk::Format::eR8G8Unorm, Attachable | Storage},              // RG8U
    {vk::Format::eR8G8Snorm, Attachable | Storage},              // RG8S
    {vk::Format::eR32G32Uint, Attachable | Storage},             // RG32UI
    {vk::Format::eUndefined, {}},                                // RGBX16F
    {vk::Format::eR32Uint, Attachable | Storage},                // R32UI
    {vk::Format::eR32Sint, Attachable | Storage},                // R32I
    {vk::Format::eAstc8x8UnormBlock, {}},                        // ASTC_2D_8X8
    {vk::Format::eUndefined, {}},                                // ASTC_2D_8X5
    {vk::Format::eUndefined, {}},                                // ASTC_2D_5X4
    {vk::Format::eUndefined, {}},                                // BGRA8_SRGB
    {vk::Format::eBc1RgbaSrgbBlock, {}},                         // DXT1_SRGB
    {vk::Format::eBc2SrgbBlock, {}},                             // DXT23_SRGB
    {vk::Format::eBc3SrgbBlock, {}},                             // DXT45_SRGB
    {vk::Format::eBc7SrgbBlock, {}},                             // BC7U_SRGB
    {vk::Format::eR4G4B4A4UnormPack16, Attachable},              // R4G4B4A4U
    {vk::Format::eAstc4x4SrgbBlock, {}},                         // ASTC_2D_4X4_SRGB
    {vk::Format::eAstc8x8SrgbBlock, {}},                         // ASTC_2D_8X8_SRGB
    {vk::Format::eAstc8x5SrgbBlock, {}},                         // ASTC_2D_8X5_SRGB
    {vk::Format::eAstc5x4SrgbBlock, {}},                         // ASTC_2D_5X4_SRGB
    {vk::Format::eAstc5x5UnormBlock, {}},                        // ASTC_2D_5X5
    {vk::Format::eAstc5x5SrgbBlock, {}},                         // ASTC_2D_5X5_SRGB
    {vk::Format::eAstc10x8UnormBlock, {}},                       // ASTC_2D_10X8
    {vk::Format::eAstc10x8SrgbBlock, {}},                        // ASTC_2D_10X8_SRGB
    {vk::Format::eAstc6x6UnormBlock, {}},                        // ASTC_2D_6X6
    {vk::Format::eAstc6x6SrgbBlock, {}},                         // ASTC_2D_6X6_SRGB
    {vk::Format::eAstc10x10UnormBlock, {}},                      // ASTC_2D_10X10
    {vk::Format::eAstc10x10SrgbBlock, {}},                       // ASTC_2D_10X10_SRGB
    {vk::Format::eAstc12x12UnormBlock, {}},                      // ASTC_2D_12X12
    {vk::Format::eAstc12x12SrgbBlock, {}},                       // ASTC_2D_12X12_SRGB
    {vk::Format::eAstc8x6UnormBlock, {}},                        // ASTC_2D_8X6
    {vk::Format::eAstc8x6SrgbBlock, {}},                         // ASTC_2D_8X6_SRGB
    {vk::Format::eAstc6x5UnormBlock, {}},                        // ASTC_2D_6X5
    {vk::Format::eAstc6x5SrgbBlock, {}},                         // ASTC_2D_6X5_SRGB
    {vk::Format::eE5B9G9R9UfloatPack32, {}},                     // E5B9G9R9F

    // Depth formats
    {vk::Format::eD32Sfloat, Attachable}, // Z32F
    {vk::Format::eD16Unorm, Attachable},  // Z16

    // DepthStencil formats
    {vk::Format::eD24UnormS8Uint, Attachable},  // Z24S8
    {vk::Format::eD24UnormS8Uint, Attachable},  // S8Z24 (emulated)
    {vk::Format::eD32SfloatS8Uint, Attachable}, // Z32FS8
};
static_assert(std::size(tex_format_tuples) == VideoCore::Surface::MaxPixelFormat);

constexpr bool IsZetaFormat(PixelFormat pixel_format) {
    return pixel_format >= PixelFormat::MaxColorFormat &&
           pixel_format < PixelFormat::MaxDepthStencilFormat;
}

} // Anonymous namespace

FormatInfo SurfaceFormat(const VKDevice& device, FormatType format_type, PixelFormat pixel_format) {
    ASSERT(static_cast<std::size_t>(pixel_format) < std::size(tex_format_tuples));

    auto tuple = tex_format_tuples[static_cast<std::size_t>(pixel_format)];
    if (tuple.format == vk::Format::eUndefined) {
        UNIMPLEMENTED_MSG("Unimplemented texture format with pixel format={}",
                          static_cast<u32>(pixel_format));
        return {vk::Format::eA8B8G8R8UnormPack32, true, true};
    }

    // Use ABGR8 on hardware that doesn't support ASTC natively
    if (!device.IsOptimalAstcSupported() && VideoCore::Surface::IsPixelFormatASTC(pixel_format)) {
        tuple.format = VideoCore::Surface::IsPixelFormatSRGB(pixel_format)
                           ? vk::Format::eA8B8G8R8SrgbPack32
                           : vk::Format::eA8B8G8R8UnormPack32;
    }
    const bool attachable = tuple.usage & Attachable;
    const bool storage = tuple.usage & Storage;

    vk::FormatFeatureFlags usage;
    if (format_type == FormatType::Buffer) {
        usage = vk::FormatFeatureFlagBits::eStorageTexelBuffer |
                vk::FormatFeatureFlagBits::eUniformTexelBuffer;
    } else {
        usage = vk::FormatFeatureFlagBits::eSampledImage | vk::FormatFeatureFlagBits::eTransferDst |
                vk::FormatFeatureFlagBits::eTransferSrc;
        if (attachable) {
            usage |= IsZetaFormat(pixel_format) ? vk::FormatFeatureFlagBits::eDepthStencilAttachment
                                                : vk::FormatFeatureFlagBits::eColorAttachment;
        }
        if (storage) {
            usage |= vk::FormatFeatureFlagBits::eStorageImage;
        }
    }
    return {device.GetSupportedFormat(tuple.format, usage, format_type), attachable, storage};
}

vk::ShaderStageFlagBits ShaderStage(Tegra::Engines::ShaderType stage) {
    switch (stage) {
    case Tegra::Engines::ShaderType::Vertex:
        return vk::ShaderStageFlagBits::eVertex;
    case Tegra::Engines::ShaderType::TesselationControl:
        return vk::ShaderStageFlagBits::eTessellationControl;
    case Tegra::Engines::ShaderType::TesselationEval:
        return vk::ShaderStageFlagBits::eTessellationEvaluation;
    case Tegra::Engines::ShaderType::Geometry:
        return vk::ShaderStageFlagBits::eGeometry;
    case Tegra::Engines::ShaderType::Fragment:
        return vk::ShaderStageFlagBits::eFragment;
    }
    UNIMPLEMENTED_MSG("Unimplemented shader stage={}", static_cast<u32>(stage));
    return {};
}

vk::PrimitiveTopology PrimitiveTopology([[maybe_unused]] const VKDevice& device,
                                        Maxwell::PrimitiveTopology topology) {
    switch (topology) {
    case Maxwell::PrimitiveTopology::Points:
        return vk::PrimitiveTopology::ePointList;
    case Maxwell::PrimitiveTopology::Lines:
        return vk::PrimitiveTopology::eLineList;
    case Maxwell::PrimitiveTopology::LineStrip:
        return vk::PrimitiveTopology::eLineStrip;
    case Maxwell::PrimitiveTopology::Triangles:
        return vk::PrimitiveTopology::eTriangleList;
    case Maxwell::PrimitiveTopology::TriangleStrip:
        return vk::PrimitiveTopology::eTriangleStrip;
    case Maxwell::PrimitiveTopology::TriangleFan:
        return vk::PrimitiveTopology::eTriangleFan;
    case Maxwell::PrimitiveTopology::Quads:
        // TODO(Rodrigo): Use VK_PRIMITIVE_TOPOLOGY_QUAD_LIST_EXT whenever it releases
        return vk::PrimitiveTopology::eTriangleList;
    case Maxwell::PrimitiveTopology::Patches:
        return vk::PrimitiveTopology::ePatchList;
    default:
        UNIMPLEMENTED_MSG("Unimplemented topology={}", static_cast<u32>(topology));
        return {};
    }
}

vk::Format VertexFormat(Maxwell::VertexAttribute::Type type, Maxwell::VertexAttribute::Size size) {
    switch (type) {
    case Maxwell::VertexAttribute::Type::SignedNorm:
        switch (size) {
        case Maxwell::VertexAttribute::Size::Size_8:
            return vk::Format::eR8Snorm;
        case Maxwell::VertexAttribute::Size::Size_8_8:
            return vk::Format::eR8G8Snorm;
        case Maxwell::VertexAttribute::Size::Size_8_8_8:
            return vk::Format::eR8G8B8Snorm;
        case Maxwell::VertexAttribute::Size::Size_8_8_8_8:
            return vk::Format::eR8G8B8A8Snorm;
        case Maxwell::VertexAttribute::Size::Size_16:
            return vk::Format::eR16Snorm;
        case Maxwell::VertexAttribute::Size::Size_16_16:
            return vk::Format::eR16G16Snorm;
        case Maxwell::VertexAttribute::Size::Size_16_16_16:
            return vk::Format::eR16G16B16Snorm;
        case Maxwell::VertexAttribute::Size::Size_16_16_16_16:
            return vk::Format::eR16G16B16A16Snorm;
        case Maxwell::VertexAttribute::Size::Size_10_10_10_2:
            return vk::Format::eA2B10G10R10SnormPack32;
        default:
            break;
        }
        break;
    case Maxwell::VertexAttribute::Type::UnsignedNorm:
        switch (size) {
        case Maxwell::VertexAttribute::Size::Size_8:
            return vk::Format::eR8Unorm;
        case Maxwell::VertexAttribute::Size::Size_8_8:
            return vk::Format::eR8G8Unorm;
        case Maxwell::VertexAttribute::Size::Size_8_8_8:
            return vk::Format::eR8G8B8Unorm;
        case Maxwell::VertexAttribute::Size::Size_8_8_8_8:
            return vk::Format::eR8G8B8A8Unorm;
        case Maxwell::VertexAttribute::Size::Size_16:
            return vk::Format::eR16Unorm;
        case Maxwell::VertexAttribute::Size::Size_16_16:
            return vk::Format::eR16G16Unorm;
        case Maxwell::VertexAttribute::Size::Size_16_16_16:
            return vk::Format::eR16G16B16Unorm;
        case Maxwell::VertexAttribute::Size::Size_16_16_16_16:
            return vk::Format::eR16G16B16A16Unorm;
        case Maxwell::VertexAttribute::Size::Size_10_10_10_2:
            return vk::Format::eA2B10G10R10UnormPack32;
        default:
            break;
        }
        break;
    case Maxwell::VertexAttribute::Type::SignedInt:
        switch (size) {
        case Maxwell::VertexAttribute::Size::Size_16_16_16_16:
            return vk::Format::eR16G16B16A16Sint;
        case Maxwell::VertexAttribute::Size::Size_8:
            return vk::Format::eR8Sint;
        case Maxwell::VertexAttribute::Size::Size_8_8:
            return vk::Format::eR8G8Sint;
        case Maxwell::VertexAttribute::Size::Size_8_8_8:
            return vk::Format::eR8G8B8Sint;
        case Maxwell::VertexAttribute::Size::Size_8_8_8_8:
            return vk::Format::eR8G8B8A8Sint;
        case Maxwell::VertexAttribute::Size::Size_32:
            return vk::Format::eR32Sint;
        default:
            break;
        }
    case Maxwell::VertexAttribute::Type::UnsignedInt:
        switch (size) {
        case Maxwell::VertexAttribute::Size::Size_8:
            return vk::Format::eR8Uint;
        case Maxwell::VertexAttribute::Size::Size_8_8:
            return vk::Format::eR8G8Uint;
        case Maxwell::VertexAttribute::Size::Size_8_8_8:
            return vk::Format::eR8G8B8Uint;
        case Maxwell::VertexAttribute::Size::Size_8_8_8_8:
            return vk::Format::eR8G8B8A8Uint;
        case Maxwell::VertexAttribute::Size::Size_32:
            return vk::Format::eR32Uint;
        case Maxwell::VertexAttribute::Size::Size_32_32_32_32:
            return vk::Format::eR32G32B32A32Uint;
        default:
            break;
        }
    case Maxwell::VertexAttribute::Type::UnsignedScaled:
        switch (size) {
        case Maxwell::VertexAttribute::Size::Size_8:
            return vk::Format::eR8Uscaled;
        case Maxwell::VertexAttribute::Size::Size_8_8:
            return vk::Format::eR8G8Uscaled;
        case Maxwell::VertexAttribute::Size::Size_8_8_8:
            return vk::Format::eR8G8B8Uscaled;
        case Maxwell::VertexAttribute::Size::Size_8_8_8_8:
            return vk::Format::eR8G8B8A8Uscaled;
        case Maxwell::VertexAttribute::Size::Size_16:
            return vk::Format::eR16Uscaled;
        case Maxwell::VertexAttribute::Size::Size_16_16:
            return vk::Format::eR16G16Uscaled;
        case Maxwell::VertexAttribute::Size::Size_16_16_16:
            return vk::Format::eR16G16B16Uscaled;
        case Maxwell::VertexAttribute::Size::Size_16_16_16_16:
            return vk::Format::eR16G16B16A16Uscaled;
        default:
            break;
        }
        break;
    case Maxwell::VertexAttribute::Type::SignedScaled:
        break;
    case Maxwell::VertexAttribute::Type::Float:
        switch (size) {
        case Maxwell::VertexAttribute::Size::Size_32:
            return vk::Format::eR32Sfloat;
        case Maxwell::VertexAttribute::Size::Size_32_32:
            return vk::Format::eR32G32Sfloat;
        case Maxwell::VertexAttribute::Size::Size_32_32_32:
            return vk::Format::eR32G32B32Sfloat;
        case Maxwell::VertexAttribute::Size::Size_32_32_32_32:
            return vk::Format::eR32G32B32A32Sfloat;
        case Maxwell::VertexAttribute::Size::Size_16:
            return vk::Format::eR16Sfloat;
        case Maxwell::VertexAttribute::Size::Size_16_16:
            return vk::Format::eR16G16Sfloat;
        case Maxwell::VertexAttribute::Size::Size_16_16_16:
            return vk::Format::eR16G16B16Sfloat;
        case Maxwell::VertexAttribute::Size::Size_16_16_16_16:
            return vk::Format::eR16G16B16A16Sfloat;
        default:
            break;
        }
        break;
    }
    UNIMPLEMENTED_MSG("Unimplemented vertex format of type={} and size={}", static_cast<u32>(type),
                      static_cast<u32>(size));
    return {};
}

vk::CompareOp ComparisonOp(Maxwell::ComparisonOp comparison) {
    switch (comparison) {
    case Maxwell::ComparisonOp::Never:
    case Maxwell::ComparisonOp::NeverOld:
        return vk::CompareOp::eNever;
    case Maxwell::ComparisonOp::Less:
    case Maxwell::ComparisonOp::LessOld:
        return vk::CompareOp::eLess;
    case Maxwell::ComparisonOp::Equal:
    case Maxwell::ComparisonOp::EqualOld:
        return vk::CompareOp::eEqual;
    case Maxwell::ComparisonOp::LessEqual:
    case Maxwell::ComparisonOp::LessEqualOld:
        return vk::CompareOp::eLessOrEqual;
    case Maxwell::ComparisonOp::Greater:
    case Maxwell::ComparisonOp::GreaterOld:
        return vk::CompareOp::eGreater;
    case Maxwell::ComparisonOp::NotEqual:
    case Maxwell::ComparisonOp::NotEqualOld:
        return vk::CompareOp::eNotEqual;
    case Maxwell::ComparisonOp::GreaterEqual:
    case Maxwell::ComparisonOp::GreaterEqualOld:
        return vk::CompareOp::eGreaterOrEqual;
    case Maxwell::ComparisonOp::Always:
    case Maxwell::ComparisonOp::AlwaysOld:
        return vk::CompareOp::eAlways;
    }
    UNIMPLEMENTED_MSG("Unimplemented comparison op={}", static_cast<u32>(comparison));
    return {};
}

vk::IndexType IndexFormat(const VKDevice& device, Maxwell::IndexFormat index_format) {
    switch (index_format) {
    case Maxwell::IndexFormat::UnsignedByte:
        if (!device.IsExtIndexTypeUint8Supported()) {
            UNIMPLEMENTED_MSG("Native uint8 indices are not supported on this device");
            return vk::IndexType::eUint16;
        }
        return vk::IndexType::eUint8EXT;
    case Maxwell::IndexFormat::UnsignedShort:
        return vk::IndexType::eUint16;
    case Maxwell::IndexFormat::UnsignedInt:
        return vk::IndexType::eUint32;
    }
    UNIMPLEMENTED_MSG("Unimplemented index_format={}", static_cast<u32>(index_format));
    return {};
}

vk::StencilOp StencilOp(Maxwell::StencilOp stencil_op) {
    switch (stencil_op) {
    case Maxwell::StencilOp::Keep:
    case Maxwell::StencilOp::KeepOGL:
        return vk::StencilOp::eKeep;
    case Maxwell::StencilOp::Zero:
    case Maxwell::StencilOp::ZeroOGL:
        return vk::StencilOp::eZero;
    case Maxwell::StencilOp::Replace:
    case Maxwell::StencilOp::ReplaceOGL:
        return vk::StencilOp::eReplace;
    case Maxwell::StencilOp::Incr:
    case Maxwell::StencilOp::IncrOGL:
        return vk::StencilOp::eIncrementAndClamp;
    case Maxwell::StencilOp::Decr:
    case Maxwell::StencilOp::DecrOGL:
        return vk::StencilOp::eDecrementAndClamp;
    case Maxwell::StencilOp::Invert:
    case Maxwell::StencilOp::InvertOGL:
        return vk::StencilOp::eInvert;
    case Maxwell::StencilOp::IncrWrap:
    case Maxwell::StencilOp::IncrWrapOGL:
        return vk::StencilOp::eIncrementAndWrap;
    case Maxwell::StencilOp::DecrWrap:
    case Maxwell::StencilOp::DecrWrapOGL:
        return vk::StencilOp::eDecrementAndWrap;
    }
    UNIMPLEMENTED_MSG("Unimplemented stencil op={}", static_cast<u32>(stencil_op));
    return {};
}

vk::BlendOp BlendEquation(Maxwell::Blend::Equation equation) {
    switch (equation) {
    case Maxwell::Blend::Equation::Add:
    case Maxwell::Blend::Equation::AddGL:
        return vk::BlendOp::eAdd;
    case Maxwell::Blend::Equation::Subtract:
    case Maxwell::Blend::Equation::SubtractGL:
        return vk::BlendOp::eSubtract;
    case Maxwell::Blend::Equation::ReverseSubtract:
    case Maxwell::Blend::Equation::ReverseSubtractGL:
        return vk::BlendOp::eReverseSubtract;
    case Maxwell::Blend::Equation::Min:
    case Maxwell::Blend::Equation::MinGL:
        return vk::BlendOp::eMin;
    case Maxwell::Blend::Equation::Max:
    case Maxwell::Blend::Equation::MaxGL:
        return vk::BlendOp::eMax;
    }
    UNIMPLEMENTED_MSG("Unimplemented blend equation={}", static_cast<u32>(equation));
    return {};
}

vk::BlendFactor BlendFactor(Maxwell::Blend::Factor factor) {
    switch (factor) {
    case Maxwell::Blend::Factor::Zero:
    case Maxwell::Blend::Factor::ZeroGL:
        return vk::BlendFactor::eZero;
    case Maxwell::Blend::Factor::One:
    case Maxwell::Blend::Factor::OneGL:
        return vk::BlendFactor::eOne;
    case Maxwell::Blend::Factor::SourceColor:
    case Maxwell::Blend::Factor::SourceColorGL:
        return vk::BlendFactor::eSrcColor;
    case Maxwell::Blend::Factor::OneMinusSourceColor:
    case Maxwell::Blend::Factor::OneMinusSourceColorGL:
        return vk::BlendFactor::eOneMinusSrcColor;
    case Maxwell::Blend::Factor::SourceAlpha:
    case Maxwell::Blend::Factor::SourceAlphaGL:
        return vk::BlendFactor::eSrcAlpha;
    case Maxwell::Blend::Factor::OneMinusSourceAlpha:
    case Maxwell::Blend::Factor::OneMinusSourceAlphaGL:
        return vk::BlendFactor::eOneMinusSrcAlpha;
    case Maxwell::Blend::Factor::DestAlpha:
    case Maxwell::Blend::Factor::DestAlphaGL:
        return vk::BlendFactor::eDstAlpha;
    case Maxwell::Blend::Factor::OneMinusDestAlpha:
    case Maxwell::Blend::Factor::OneMinusDestAlphaGL:
        return vk::BlendFactor::eOneMinusDstAlpha;
    case Maxwell::Blend::Factor::DestColor:
    case Maxwell::Blend::Factor::DestColorGL:
        return vk::BlendFactor::eDstColor;
    case Maxwell::Blend::Factor::OneMinusDestColor:
    case Maxwell::Blend::Factor::OneMinusDestColorGL:
        return vk::BlendFactor::eOneMinusDstColor;
    case Maxwell::Blend::Factor::SourceAlphaSaturate:
    case Maxwell::Blend::Factor::SourceAlphaSaturateGL:
        return vk::BlendFactor::eSrcAlphaSaturate;
    case Maxwell::Blend::Factor::Source1Color:
    case Maxwell::Blend::Factor::Source1ColorGL:
        return vk::BlendFactor::eSrc1Color;
    case Maxwell::Blend::Factor::OneMinusSource1Color:
    case Maxwell::Blend::Factor::OneMinusSource1ColorGL:
        return vk::BlendFactor::eOneMinusSrc1Color;
    case Maxwell::Blend::Factor::Source1Alpha:
    case Maxwell::Blend::Factor::Source1AlphaGL:
        return vk::BlendFactor::eSrc1Alpha;
    case Maxwell::Blend::Factor::OneMinusSource1Alpha:
    case Maxwell::Blend::Factor::OneMinusSource1AlphaGL:
        return vk::BlendFactor::eOneMinusSrc1Alpha;
    case Maxwell::Blend::Factor::ConstantColor:
    case Maxwell::Blend::Factor::ConstantColorGL:
        return vk::BlendFactor::eConstantColor;
    case Maxwell::Blend::Factor::OneMinusConstantColor:
    case Maxwell::Blend::Factor::OneMinusConstantColorGL:
        return vk::BlendFactor::eOneMinusConstantColor;
    case Maxwell::Blend::Factor::ConstantAlpha:
    case Maxwell::Blend::Factor::ConstantAlphaGL:
        return vk::BlendFactor::eConstantAlpha;
    case Maxwell::Blend::Factor::OneMinusConstantAlpha:
    case Maxwell::Blend::Factor::OneMinusConstantAlphaGL:
        return vk::BlendFactor::eOneMinusConstantAlpha;
    }
    UNIMPLEMENTED_MSG("Unimplemented blend factor={}", static_cast<u32>(factor));
    return {};
}

vk::FrontFace FrontFace(Maxwell::FrontFace front_face) {
    switch (front_face) {
    case Maxwell::FrontFace::ClockWise:
        return vk::FrontFace::eClockwise;
    case Maxwell::FrontFace::CounterClockWise:
        return vk::FrontFace::eCounterClockwise;
    }
    UNIMPLEMENTED_MSG("Unimplemented front face={}", static_cast<u32>(front_face));
    return {};
}

vk::CullModeFlags CullFace(Maxwell::CullFace cull_face) {
    switch (cull_face) {
    case Maxwell::CullFace::Front:
        return vk::CullModeFlagBits::eFront;
    case Maxwell::CullFace::Back:
        return vk::CullModeFlagBits::eBack;
    case Maxwell::CullFace::FrontAndBack:
        return vk::CullModeFlagBits::eFrontAndBack;
    }
    UNIMPLEMENTED_MSG("Unimplemented cull face={}", static_cast<u32>(cull_face));
    return {};
}

vk::ComponentSwizzle SwizzleSource(Tegra::Texture::SwizzleSource swizzle) {
    switch (swizzle) {
    case Tegra::Texture::SwizzleSource::Zero:
        return vk::ComponentSwizzle::eZero;
    case Tegra::Texture::SwizzleSource::R:
        return vk::ComponentSwizzle::eR;
    case Tegra::Texture::SwizzleSource::G:
        return vk::ComponentSwizzle::eG;
    case Tegra::Texture::SwizzleSource::B:
        return vk::ComponentSwizzle::eB;
    case Tegra::Texture::SwizzleSource::A:
        return vk::ComponentSwizzle::eA;
    case Tegra::Texture::SwizzleSource::OneInt:
    case Tegra::Texture::SwizzleSource::OneFloat:
        return vk::ComponentSwizzle::eOne;
    }
    UNIMPLEMENTED_MSG("Unimplemented swizzle source={}", static_cast<u32>(swizzle));
    return {};
}

} // namespace Vulkan::MaxwellToVK
