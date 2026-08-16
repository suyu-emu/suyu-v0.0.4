// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2025 lsfg-vk
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/present/lsfg_common.h"

namespace Vulkan {

class Device;
class LsfgShaders;

constexpr size_t LSFG_MIP_LEVELS = 7;

class LsfgMipmaps {
public:
    LsfgMipmaps() = default;
    LsfgMipmaps(const Device& device, MemoryAllocator& memory_allocator, const LsfgShaders& shaders,
                LsfgResources& resources, vk::DescriptorPool& descriptor_pool,
                LsfgImagePair& frames, f32 flow_scale);

    void Dispatch(vk::CommandBuffer cmdbuf, u64 frame_count);

    [[nodiscard]] LsfgImage& Output(size_t level) {
        return out_images[level];
    }

private:
    LsfgImagePair* frames{};

    LsfgPass pass;
    std::array<VkDescriptorSet, 2> descriptor_sets{};
    vk::DescriptorSets owned_sets;

    VkExtent2D flow_extent{};
    std::array<LsfgImage, LSFG_MIP_LEVELS> out_images;
};

} // namespace Vulkan
