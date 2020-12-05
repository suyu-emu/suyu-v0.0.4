// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/sampler_cache.h"
#include "video_core/textures/texture.h"

namespace Vulkan {

class VKDevice;

class VKSamplerCache final : public VideoCommon::SamplerCache<VkSampler, vk::Sampler> {
public:
    explicit VKSamplerCache(const VKDevice& device_);
    ~VKSamplerCache();

protected:
    vk::Sampler CreateSampler(const Tegra::Texture::TSCEntry& tsc) const override;

    VkSampler ToSamplerType(const vk::Sampler& sampler) const override;

private:
    const VKDevice& device;
};

} // namespace Vulkan
