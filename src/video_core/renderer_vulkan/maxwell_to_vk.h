// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/surface.h"
#include "video_core/textures/texture.h"

namespace Vulkan::MaxwellToVK {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using PixelFormat = VideoCore::Surface::PixelFormat;

namespace Sampler {

VkFilter Filter(Tegra::Texture::TextureFilter filter);

VkSamplerMipmapMode MipmapMode(Tegra::Texture::TextureMipmapFilter mipmap_filter);

VkSamplerAddressMode WrapMode(const VKDevice& device, Tegra::Texture::WrapMode wrap_mode,
                              Tegra::Texture::TextureFilter filter);

VkCompareOp DepthCompareFunction(Tegra::Texture::DepthCompareFunc depth_compare_func);

} // namespace Sampler

struct FormatInfo {
    VkFormat format;
    bool attachable;
    bool storage;
};

FormatInfo SurfaceFormat(const VKDevice& device, FormatType format_type, PixelFormat pixel_format);

VkShaderStageFlagBits ShaderStage(Tegra::Engines::ShaderType stage);

VkPrimitiveTopology PrimitiveTopology(const VKDevice& device, Maxwell::PrimitiveTopology topology);

VkFormat VertexFormat(Maxwell::VertexAttribute::Type type, Maxwell::VertexAttribute::Size size);

VkCompareOp ComparisonOp(Maxwell::ComparisonOp comparison);

VkIndexType IndexFormat(const VKDevice& device, Maxwell::IndexFormat index_format);

VkStencilOp StencilOp(Maxwell::StencilOp stencil_op);

VkBlendOp BlendEquation(Maxwell::Blend::Equation equation);

VkBlendFactor BlendFactor(Maxwell::Blend::Factor factor);

VkFrontFace FrontFace(Maxwell::FrontFace front_face);

VkCullModeFlags CullFace(Maxwell::CullFace cull_face);

VkComponentSwizzle SwizzleSource(Tegra::Texture::SwizzleSource swizzle);

} // namespace Vulkan::MaxwellToVK
