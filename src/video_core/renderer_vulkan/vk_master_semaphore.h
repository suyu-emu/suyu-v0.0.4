// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

#include "common/common_types.h"
#include "common/polyfill_thread.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;

class MasterSemaphore {
public:
    explicit MasterSemaphore(const Device& device);
    ~MasterSemaphore();

    /// Returns the current logical tick.
    [[nodiscard]] u64 CurrentTick() const noexcept {
        return current_tick.load(std::memory_order_acquire);
    }

    /// Returns the last known GPU tick.
    [[nodiscard]] u64 KnownGpuTick() const noexcept {
        return gpu_tick.load(std::memory_order_acquire);
    }

    /// Returns true when a tick has been hit by the GPU.
    [[nodiscard]] bool IsFree(u64 tick) const noexcept {
        return KnownGpuTick() >= tick;
    }

    /// Advance to the logical tick and return the old one
    [[nodiscard]] u64 NextTick() noexcept {
        return current_tick.fetch_add(1, std::memory_order_release);
    }

    /// Refresh the known GPU tick
    void Refresh();

    /// Waits for a tick to be hit on the GPU
    void Wait(u64 tick);

    /// Submits the device graphics queue, updating the tick as necessary
    VkResult SubmitQueue(vk::CommandBuffer& cmdbuf, VkSemaphore signal_semaphore,
                         VkSemaphore wait_semaphore, u64 host_tick);

private:
    VkResult SubmitQueueTimeline(vk::CommandBuffer& cmdbuf, VkSemaphore signal_semaphore,
                                 VkSemaphore wait_semaphore, u64 host_tick);
    VkResult SubmitQueueFence(vk::CommandBuffer& cmdbuf, VkSemaphore signal_semaphore,
                              VkSemaphore wait_semaphore, u64 host_tick);

private:
    const Device& device;             ///< Device.
    vk::Fence fence;                  ///< Fence.
    vk::Semaphore semaphore;          ///< Timeline semaphore.
    std::atomic<u64> gpu_tick{0};     ///< Current known GPU tick.
    std::atomic<u64> current_tick{1}; ///< Current logical tick.
    std::jthread debug_thread;        ///< Debug thread to workaround validation layer bugs.
};

} // namespace Vulkan
