// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <thread>

#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_fence_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

InnerFence::InnerFence(const VKDevice& device_, VKScheduler& scheduler_, u32 payload_,
                       bool is_stubbed_)
    : FenceBase{payload_, is_stubbed_}, device{device_}, scheduler{scheduler_} {}

InnerFence::InnerFence(const VKDevice& device_, VKScheduler& scheduler_, GPUVAddr address_,
                       u32 payload_, bool is_stubbed_)
    : FenceBase{address_, payload_, is_stubbed_}, device{device_}, scheduler{scheduler_} {}

InnerFence::~InnerFence() = default;

void InnerFence::Queue() {
    if (is_stubbed) {
        return;
    }
    ASSERT(!event);

    event = device.GetLogical().CreateEvent();
    ticks = scheduler.CurrentTick();

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([event = *event](vk::CommandBuffer cmdbuf) {
        cmdbuf.SetEvent(event, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    });
}

bool InnerFence::IsSignaled() const {
    if (is_stubbed) {
        return true;
    }
    ASSERT(event);
    return IsEventSignalled();
}

void InnerFence::Wait() {
    if (is_stubbed) {
        return;
    }
    ASSERT(event);

    if (ticks >= scheduler.CurrentTick()) {
        scheduler.Flush();
    }
    while (!IsEventSignalled()) {
        std::this_thread::yield();
    }
}

bool InnerFence::IsEventSignalled() const {
    switch (const VkResult result = event.GetStatus()) {
    case VK_EVENT_SET:
        return true;
    case VK_EVENT_RESET:
        return false;
    default:
        throw vk::Exception(result);
    }
}

VKFenceManager::VKFenceManager(VideoCore::RasterizerInterface& rasterizer_, Tegra::GPU& gpu_,
                               Tegra::MemoryManager& memory_manager_,
                               VKTextureCache& texture_cache_, VKBufferCache& buffer_cache_,
                               VKQueryCache& query_cache_, const VKDevice& device_,
                               VKScheduler& scheduler_)
    : GenericFenceManager{rasterizer_, gpu_, texture_cache_, buffer_cache_, query_cache_},
      device{device_}, scheduler{scheduler_} {}

Fence VKFenceManager::CreateFence(u32 value, bool is_stubbed) {
    return std::make_shared<InnerFence>(device, scheduler, value, is_stubbed);
}

Fence VKFenceManager::CreateFence(GPUVAddr addr, u32 value, bool is_stubbed) {
    return std::make_shared<InnerFence>(device, scheduler, addr, value, is_stubbed);
}

void VKFenceManager::QueueFence(Fence& fence) {
    fence->Queue();
}

bool VKFenceManager::IsFenceSignaled(Fence& fence) const {
    return fence->IsSignaled();
}

void VKFenceManager::WaitFence(Fence& fence) {
    fence->Wait();
}

} // namespace Vulkan
