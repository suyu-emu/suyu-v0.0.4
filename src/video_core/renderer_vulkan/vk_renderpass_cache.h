// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include <unordered_map>

#include <boost/container/static_vector.hpp>
#include <boost/functional/hash.hpp>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/wrapper.h"
#include "video_core/surface.h"

namespace Vulkan {

class VKDevice;

struct RenderPassParams {
    std::array<u8, Tegra::Engines::Maxwell3D::Regs::NumRenderTargets> color_formats;
    u8 num_color_attachments;
    u8 texceptions;

    u8 zeta_format;
    u8 zeta_texception;

    std::size_t Hash() const noexcept;

    bool operator==(const RenderPassParams& rhs) const noexcept;

    bool operator!=(const RenderPassParams& rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(std::has_unique_object_representations_v<RenderPassParams>);
static_assert(std::is_trivially_copyable_v<RenderPassParams>);
static_assert(std::is_trivially_constructible_v<RenderPassParams>);

} // namespace Vulkan

namespace std {

template <>
struct hash<Vulkan::RenderPassParams> {
    std::size_t operator()(const Vulkan::RenderPassParams& k) const noexcept {
        return k.Hash();
    }
};

} // namespace std

namespace Vulkan {

class VKRenderPassCache final {
public:
    explicit VKRenderPassCache(const VKDevice& device_);
    ~VKRenderPassCache();

    VkRenderPass GetRenderPass(const RenderPassParams& params);

private:
    vk::RenderPass CreateRenderPass(const RenderPassParams& params) const;

    const VKDevice& device;
    std::unordered_map<RenderPassParams, vk::RenderPass> cache;
};

} // namespace Vulkan
