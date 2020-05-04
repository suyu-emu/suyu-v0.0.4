// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>

#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_sampler_cache.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/textures/texture.h"

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

    VkSamplerCustomBorderColorCreateInfoEXT border;
    border.sType = VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT;
    border.pNext = nullptr;
    border.format = VK_FORMAT_UNDEFINED;
    std::memcpy(&border.customBorderColor, color.data(), sizeof(color));

    VkSamplerCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    ci.pNext = arbitrary_borders ? &border : nullptr;
    ci.flags = 0;
    ci.magFilter = MaxwellToVK::Sampler::Filter(tsc.mag_filter);
    ci.minFilter = MaxwellToVK::Sampler::Filter(tsc.min_filter);
    ci.mipmapMode = MaxwellToVK::Sampler::MipmapMode(tsc.mipmap_filter);
    ci.addressModeU = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_u, tsc.mag_filter);
    ci.addressModeV = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_v, tsc.mag_filter);
    ci.addressModeW = MaxwellToVK::Sampler::WrapMode(device, tsc.wrap_p, tsc.mag_filter);
    ci.mipLodBias = tsc.GetLodBias();
    ci.anisotropyEnable = tsc.GetMaxAnisotropy() > 1.0f ? VK_TRUE : VK_FALSE;
    ci.maxAnisotropy = tsc.GetMaxAnisotropy();
    ci.compareEnable = tsc.depth_compare_enabled;
    ci.compareOp = MaxwellToVK::Sampler::DepthCompareFunction(tsc.depth_compare_func);
    ci.minLod = tsc.GetMinLod();
    ci.maxLod = tsc.GetMaxLod();
    ci.borderColor = arbitrary_borders ? VK_BORDER_COLOR_INT_CUSTOM_EXT : ConvertBorderColor(color);
    ci.unnormalizedCoordinates = VK_FALSE;
    return device.GetLogical().CreateSampler(ci);
}

VkSampler VKSamplerCache::ToSamplerType(const vk::Sampler& sampler) const {
    return *sampler;
}

} // namespace Vulkan
