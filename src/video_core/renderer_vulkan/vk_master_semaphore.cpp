// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <thread>

#include "common/settings.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

MasterSemaphore::MasterSemaphore(const Device& device_) : device(device_) {
    if (!device.HasTimelineSemaphore()) {
        static constexpr VkFenceCreateInfo fence_ci{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .pNext = nullptr, .flags = 0};
        fence = device.GetLogical().CreateFence(fence_ci);
        return;
    }

    static constexpr VkSemaphoreTypeCreateInfo semaphore_type_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
        .pNext = nullptr,
        .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
        .initialValue = 0,
    };
    static constexpr VkSemaphoreCreateInfo semaphore_ci{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = &semaphore_type_ci,
        .flags = 0,
    };
    semaphore = device.GetLogical().CreateSemaphore(semaphore_ci);

    if (!Settings::values.renderer_debug) {
        return;
    }
    // Validation layers have a bug where they fail to track resource usage when using timeline
    // semaphores and synchronizing with GetSemaphoreCounterValue. To workaround this issue, have
    // a separate thread waiting for each timeline semaphore value.
    debug_thread = std::jthread([this](std::stop_token stop_token) {
        u64 counter = 0;
        while (!stop_token.stop_requested()) {
            if (semaphore.Wait(counter, 10'000'000)) {
                ++counter;
            }
        }
    });
}

MasterSemaphore::~MasterSemaphore() = default;

void MasterSemaphore::Refresh() {
    if (!semaphore) {
        // If we don't support timeline semaphores, there's nothing to refresh
        return;
    }

    u64 this_tick{};
    u64 counter{};
    do {
        this_tick = gpu_tick.load(std::memory_order_acquire);
        counter = semaphore.GetCounter();
        if (counter < this_tick) {
            return;
        }
    } while (!gpu_tick.compare_exchange_weak(this_tick, counter, std::memory_order_release,
                                             std::memory_order_relaxed));
}

void MasterSemaphore::Wait(u64 tick) {
    if (!semaphore) {
        // If we don't support timeline semaphores, use an atomic wait
        while (true) {
            u64 current_value = gpu_tick.load(std::memory_order_relaxed);
            if (current_value >= tick) {
                return;
            }
            gpu_tick.wait(current_value);
        }

        return;
    }

    // No need to wait if the GPU is ahead of the tick
    if (IsFree(tick)) {
        return;
    }

    // Update the GPU tick and try again
    Refresh();

    if (IsFree(tick)) {
        return;
    }

    // If none of the above is hit, fallback to a regular wait
    while (!semaphore.Wait(tick)) {
    }

    Refresh();
}

VkResult MasterSemaphore::SubmitQueue(vk::CommandBuffer& cmdbuf, VkSemaphore signal_semaphore,
                                      VkSemaphore wait_semaphore, u64 host_tick) {
    if (semaphore) {
        return SubmitQueueTimeline(cmdbuf, signal_semaphore, wait_semaphore, host_tick);
    } else {
        return SubmitQueueFence(cmdbuf, signal_semaphore, wait_semaphore, host_tick);
    }
}

static constexpr std::array<VkPipelineStageFlags, 2> wait_stage_masks{
    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
};

VkResult MasterSemaphore::SubmitQueueTimeline(vk::CommandBuffer& cmdbuf,
                                              VkSemaphore signal_semaphore,
                                              VkSemaphore wait_semaphore, u64 host_tick) {
    const VkSemaphore timeline_semaphore = *semaphore;

    const u32 num_signal_semaphores = signal_semaphore ? 2 : 1;
    const std::array signal_values{host_tick, u64(0)};
    const std::array signal_semaphores{timeline_semaphore, signal_semaphore};

    const u32 num_wait_semaphores = wait_semaphore ? 2 : 1;
    const std::array wait_values{host_tick - 1, u64(1)};
    const std::array wait_semaphores{timeline_semaphore, wait_semaphore};

    const VkTimelineSemaphoreSubmitInfo timeline_si{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreValueCount = num_wait_semaphores,
        .pWaitSemaphoreValues = wait_values.data(),
        .signalSemaphoreValueCount = num_signal_semaphores,
        .pSignalSemaphoreValues = signal_values.data(),
    };
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &timeline_si,
        .waitSemaphoreCount = num_wait_semaphores,
        .pWaitSemaphores = wait_semaphores.data(),
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = cmdbuf.address(),
        .signalSemaphoreCount = num_signal_semaphores,
        .pSignalSemaphores = signal_semaphores.data(),
    };

    return device.GetGraphicsQueue().Submit(submit_info);
}

VkResult MasterSemaphore::SubmitQueueFence(vk::CommandBuffer& cmdbuf, VkSemaphore signal_semaphore,
                                           VkSemaphore wait_semaphore, u64 host_tick) {
    const u32 num_signal_semaphores = signal_semaphore ? 1 : 0;
    const u32 num_wait_semaphores = wait_semaphore ? 1 : 0;

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = num_wait_semaphores,
        .pWaitSemaphores = &wait_semaphore,
        .pWaitDstStageMask = wait_stage_masks.data(),
        .commandBufferCount = 1,
        .pCommandBuffers = cmdbuf.address(),
        .signalSemaphoreCount = num_signal_semaphores,
        .pSignalSemaphores = &signal_semaphore,
    };

    auto result = device.GetGraphicsQueue().Submit(submit_info, *fence);

    if (result == VK_SUCCESS) {
        fence.Wait();
        fence.Reset();
        gpu_tick.store(host_tick);
        gpu_tick.notify_all();
    }

    return result;
}

} // namespace Vulkan
