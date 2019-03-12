// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/textures/texture.h"

namespace Vulkan {

class VKDevice;

struct SamplerCacheKey final : public Tegra::Texture::TSCEntry {
    std::size_t Hash() const;

    bool operator==(const SamplerCacheKey& rhs) const;

    bool operator!=(const SamplerCacheKey& rhs) const {
        return !operator==(rhs);
    }
};

} // namespace Vulkan

namespace std {

template <>
struct hash<Vulkan::SamplerCacheKey> {
    std::size_t operator()(const Vulkan::SamplerCacheKey& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std

namespace Vulkan {

class VKSamplerCache {
public:
    explicit VKSamplerCache(const VKDevice& device);
    ~VKSamplerCache();

    vk::Sampler GetSampler(const Tegra::Texture::TSCEntry& tsc);

private:
    UniqueSampler CreateSampler(const Tegra::Texture::TSCEntry& tsc);

    const VKDevice& device;
    std::unordered_map<SamplerCacheKey, UniqueSampler> cache;
};

} // namespace Vulkan
