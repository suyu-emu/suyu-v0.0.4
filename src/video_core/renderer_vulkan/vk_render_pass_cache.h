// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>
#include <ankerl/unordered_dense.h>

#include "video_core/surface.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

struct RenderPassKey {
    bool operator==(const RenderPassKey&) const noexcept = default;

    std::array<VideoCore::Surface::PixelFormat, 8> color_formats;
    VideoCore::Surface::PixelFormat depth_format;
    VkSampleCountFlagBits samples;
    bool resolve_color;
    u32 color_clear_mask;
    bool depth_stencil_clear;
    u32 color_discard_mask;
};

} // namespace Vulkan

namespace std {
template <>
struct hash<Vulkan::RenderPassKey> {
    [[nodiscard]] size_t operator()(const Vulkan::RenderPassKey& key) const noexcept {
        size_t value = static_cast<size_t>(key.depth_format) << 48;
        value ^= static_cast<size_t>(key.samples) << 52;
        value ^= static_cast<size_t>(key.resolve_color) << 63;
        value ^= static_cast<size_t>(key.color_clear_mask) << 54;
        value ^= static_cast<size_t>(key.depth_stencil_clear) << 62;
        value ^= static_cast<size_t>(key.color_discard_mask) << 24;
        for (size_t i = 0; i < key.color_formats.size(); ++i) {
            value ^= static_cast<size_t>(key.color_formats[i]) << (i * 6);
        }
        return value;
    }
};
} // namespace std

namespace Vulkan {

class Device;

class RenderPassCache {
public:
    explicit RenderPassCache(const Device& device_);

    VkRenderPass Get(const RenderPassKey& key);

private:
    const Device* device{};
    ankerl::unordered_dense::map<RenderPassKey, vk::RenderPass> cache;
    std::mutex mutex;
};

} // namespace Vulkan
