// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

#include "common/microprofile.h"
#include "common/thread.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_state_tracker.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

MICROPROFILE_DECLARE(Vulkan_WaitForWorker);

void VKScheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf);
        command->~Command();
        command = next;
    }

    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

VKScheduler::VKScheduler(const VKDevice& device, VKResourceManager& resource_manager,
                         StateTracker& state_tracker)
    : device{device}, resource_manager{resource_manager}, state_tracker{state_tracker},
      next_fence{&resource_manager.CommitFence()} {
    AcquireNewChunk();
    AllocateNewContext();
    worker_thread = std::thread(&VKScheduler::WorkerThread, this);
}

VKScheduler::~VKScheduler() {
    quit = true;
    cv.notify_all();
    worker_thread.join();
}

void VKScheduler::Flush(bool release_fence, VkSemaphore semaphore) {
    SubmitExecution(semaphore);
    if (release_fence) {
        current_fence->Release();
    }
    AllocateNewContext();
}

void VKScheduler::Finish(bool release_fence, VkSemaphore semaphore) {
    SubmitExecution(semaphore);
    current_fence->Wait();
    if (release_fence) {
        current_fence->Release();
    }
    AllocateNewContext();
}

void VKScheduler::WaitWorker() {
    MICROPROFILE_SCOPE(Vulkan_WaitForWorker);
    DispatchWork();

    bool finished = false;
    do {
        cv.notify_all();
        std::unique_lock lock{mutex};
        finished = chunk_queue.Empty();
    } while (!finished);
}

void VKScheduler::DispatchWork() {
    if (chunk->Empty()) {
        return;
    }
    chunk_queue.Push(std::move(chunk));
    cv.notify_all();
    AcquireNewChunk();
}

void VKScheduler::RequestRenderpass(VkRenderPass renderpass, VkFramebuffer framebuffer,
                                    VkExtent2D render_area) {
    if (renderpass == state.renderpass && framebuffer == state.framebuffer &&
        render_area.width == state.render_area.width &&
        render_area.height == state.render_area.height) {
        return;
    }
    const bool end_renderpass = state.renderpass != nullptr;
    state.renderpass = renderpass;
    state.framebuffer = framebuffer;
    state.render_area = render_area;

    const VkRenderPassBeginInfo renderpass_bi{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .pNext = nullptr,
        .renderPass = renderpass,
        .framebuffer = framebuffer,
        .renderArea =
            {
                .offset = {.x = 0, .y = 0},
                .extent = render_area,
            },
        .clearValueCount = 0,
        .pClearValues = nullptr,
    };

    Record([renderpass_bi, end_renderpass](vk::CommandBuffer cmdbuf) {
        if (end_renderpass) {
            cmdbuf.EndRenderPass();
        }
        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
    });
}

void VKScheduler::RequestOutsideRenderPassOperationContext() {
    EndRenderPass();
}

void VKScheduler::BindGraphicsPipeline(VkPipeline pipeline) {
    if (state.graphics_pipeline == pipeline) {
        return;
    }
    state.graphics_pipeline = pipeline;
    Record([pipeline](vk::CommandBuffer cmdbuf) {
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    });
}

void VKScheduler::WorkerThread() {
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    std::unique_lock lock{mutex};
    do {
        cv.wait(lock, [this] { return !chunk_queue.Empty() || quit; });
        if (quit) {
            continue;
        }
        auto extracted_chunk = std::move(chunk_queue.Front());
        chunk_queue.Pop();
        extracted_chunk->ExecuteAll(current_cmdbuf);
        chunk_reserve.Push(std::move(extracted_chunk));
    } while (!quit);
}

void VKScheduler::SubmitExecution(VkSemaphore semaphore) {
    EndPendingOperations();
    InvalidateState();
    WaitWorker();

    std::unique_lock lock{mutex};

    current_cmdbuf.End();

    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = current_cmdbuf.address(),
        .signalSemaphoreCount = semaphore ? 1U : 0U,
        .pSignalSemaphores = &semaphore,
    };
    switch (const VkResult result = device.GetGraphicsQueue().Submit(submit_info, *current_fence)) {
    case VK_SUCCESS:
        break;
    case VK_ERROR_DEVICE_LOST:
        device.ReportLoss();
        [[fallthrough]];
    default:
        vk::Check(result);
    }
}

void VKScheduler::AllocateNewContext() {
    ++ticks;

    std::unique_lock lock{mutex};
    current_fence = next_fence;
    next_fence = &resource_manager.CommitFence();

    current_cmdbuf = vk::CommandBuffer(resource_manager.CommitCommandBuffer(*current_fence),
                                       device.GetDispatchLoader());
    current_cmdbuf.Begin({
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr,
    });

    // Enable counters once again. These are disabled when a command buffer is finished.
    if (query_cache) {
        query_cache->UpdateCounters();
    }
}

void VKScheduler::InvalidateState() {
    state.graphics_pipeline = nullptr;
    state_tracker.InvalidateCommandBufferState();
}

void VKScheduler::EndPendingOperations() {
    query_cache->DisableStreams();
    EndRenderPass();
}

void VKScheduler::EndRenderPass() {
    if (!state.renderpass) {
        return;
    }
    state.renderpass = nullptr;
    Record([](vk::CommandBuffer cmdbuf) { cmdbuf.EndRenderPass(); });
}

void VKScheduler::AcquireNewChunk() {
    if (chunk_reserve.Empty()) {
        chunk = std::make_unique<CommandChunk>();
        return;
    }
    chunk = std::move(chunk_reserve.Front());
    chunk_reserve.Pop();
}

} // namespace Vulkan
