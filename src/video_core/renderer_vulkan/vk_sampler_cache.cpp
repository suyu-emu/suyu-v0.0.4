// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>

#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_sampler_cache.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/textures/texture.h"

using Tegra::Texture::TextureMipmapFilter;

namespace Vulkan {

namespace {

VkBorderColor ConvertBorderColor(std::array<float, 4> color) {
    // TODO(Rodrigo): Manage integer border colors
    if (color == std::array<float, 4>{0, 0, 0, 0}) {
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    } else if (color == std::array<float, 4>{0, 0, 0, 1}) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    } else if (color == std::array<float, 4>{1, 1, 1, 1}) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    }
    if (color[0] + color[1] + color[2] > 1.35f) {
        // If color elements are brighter than roughly 0.5 average, use white border
        return VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    } else if (color[3] > 0.5f) {
        return VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    } else {
        return VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
    }
}

} // Anonymous namespace

VKSamplerCache::VKSamplerCache(const VKDevice& device) : device{device} {}

VKSamplerCache::~VKSamplerCache() = default;

vk::Sampler VKSamplerCache::CreateSampler(const Tegra::Texture::TSCEntry& tsc) const {
    const bool arbitrary_borders = device.IsExtCustomBorderColorSupported();
    const std::array color = tsc.GetBorderColor();

    VkSamplerCustomBorderColorCreateInfoEXT border{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT,
        .pNext = nullptr,
        .format = VK_FORMAT_UNDEFINED,
    };
    std::memcpy(&border.customBorderColor, color.data(), sizeof(color));

    return device.GetLogical().CreateSampler({
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = arbitrary_borders ? &border : nullptr,
        .flags = 0,
        .magFilter = MaxwellToVK::Sampler::Filter(tsc.mag_filter),
        .minFilter = MaxwellToVK::Sampler::Filter(tsc.min_filter),
        .mipmapMode = MaxwellToVK::Sampler::MipmapMode(tsc.mipmap_filter),
        .addressModeU = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_u, tsc.mag_filter),
        .addressModeV = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_v, tsc.mag_filter),
        .addressModeW = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_p, tsc.mag_filter),
        .mipLodBias = tsc.GetLodBias(),
        .anisotropyEnable =
            static_cast<VkBool32>(tsc.GetMaxAnisotropy() > 1.0f ? VK_TRUE : VK_FALSE),
        .maxAnisotropy = tsc.GetMaxAnisotropy(),
        .compareEnable = tsc.depth_compare_enabled,
        .compareOp = MaxwellToVK::Sampler::DepthCompareFunction(tsc.depth_compare_func),
        .minLod = tsc.mipmap_filter == TextureMipmapFilter::None ? 0.0f : tsc.GetMinLod(),
        .maxLod = tsc.mipmap_filter == TextureMipmapFilter::None ? 0.25f : tsc.GetMaxLod(),
        .borderColor =
            arbitrary_borders ? VK_BORDER_COLOR_INT_CUSTOM_EXT : ConvertBorderColor(color),
        .unnormalizedCoordinates = VK_FALSE,
    });
}

VkSampler VKSamplerCache::ToSamplerType(const vk::Sampler& sampler) const {
    return *sampler;
}

} // namespace Vulkan
