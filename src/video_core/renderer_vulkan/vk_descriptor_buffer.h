// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <vector>

#include "common/alignment.h"
#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;

class DescriptorBufferRing final {
    static constexpr size_t FRAMES_IN_FLIGHT = 8;
    static constexpr VkDeviceSize TILER_FRAME_SIZE = 2 * 1024 * 1024;
    static constexpr VkDeviceSize DESKTOP_FRAME_SIZE = 4 * 1024 * 1024;

public:
    explicit DescriptorBufferRing(const Device& device_, MemoryAllocator& memory_allocator);
    ~DescriptorBufferRing();

    struct Allocation {
        u8* host{};
        VkDeviceSize offset{};
        u32 chunk{};
        u64 generation{};
    };

    [[nodiscard]] u64 CurrentGeneration() const noexcept {
        return generation;
    }

    void TouchFrame(Scheduler& scheduler);

    [[nodiscard]] bool CanAllocate(VkDeviceSize size) const noexcept {
        return Common::AlignUp(size, alignment) <= chunk_capacity;
    }

    void TickFrame();

    [[nodiscard]] Allocation Allocate(Scheduler& scheduler, VkDeviceSize size);

    [[nodiscard]] VkDescriptorBufferBindingInfoEXT BindingInfo(u32 chunk) const noexcept;

    [[nodiscard]] bool IsValid() const noexcept {
        return !chunks.empty();
    }

private:
    const Device& device;
    std::vector<vk::Buffer> chunks;
    std::vector<VkDeviceAddress> chunk_addresses;
    std::vector<u8*> chunk_hosts;
    VkDeviceSize alignment{1};
    VkDeviceSize chunk_capacity{};
    size_t chunks_per_frame{};
    size_t frame_index{};
    size_t chunk_cursor{};
    VkDeviceSize cursor{};
    u64 generation{1};
    std::array<u64, FRAMES_IN_FLIGHT> frame_ticks{};
    bool frame_reused{};
};

} // namespace Vulkan
