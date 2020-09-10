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

InnerFence::InnerFence(const VKDevice& device, VKScheduler& scheduler, u32 payload, bool is_stubbed)
    : VideoCommon::FenceBase(payload, is_stubbed), device{device}, scheduler{scheduler} {}

InnerFence::InnerFence(const VKDevice& device, VKScheduler& scheduler, GPUVAddr address,
                       u32 payload, bool is_stubbed)
    : VideoCommon::FenceBase(address, payload, is_stubbed), device{device}, scheduler{scheduler} {}

InnerFence::~InnerFence() = default;

void InnerFence::Queue() {
    if (is_stubbed) {
        return;
    }
    ASSERT(!event);

    event = device.GetLogical().CreateNewEvent();
    ticks = scheduler.Ticks();

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

    if (ticks >= scheduler.Ticks()) {
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

VKFenceManager::VKFenceManager(VideoCore::RasterizerInterface& rasterizer, Tegra::GPU& gpu,
                               Tegra::MemoryManager& memory_manager, VKTextureCache& texture_cache,
                               VKBufferCache& buffer_cache, VKQueryCache& query_cache,
                               const VKDevice& device_, VKScheduler& scheduler_)
    : GenericFenceManager(rasterizer, gpu, texture_cache, buffer_cache, query_cache),
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
