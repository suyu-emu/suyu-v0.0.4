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
#include "video_core/renderer_vulkan/vk_command_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_master_semaphore.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
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

VKScheduler::VKScheduler(const VKDevice& device_, StateTracker& state_tracker_)
    : device{device_}, state_tracker{state_tracker_},
      master_semaphore{std::make_unique<MasterSemaphore>(device)},
      command_pool{std::make_unique<CommandPool>(*master_semaphore, device)} {
    AcquireNewChunk();
    AllocateNewContext();
    worker_thread = std::thread(&VKScheduler::WorkerThread, this);
}

VKScheduler::~VKScheduler() {
    quit = true;
    cv.notify_all();
    worker_thread.join();
}

u64 VKScheduler::CurrentTick() const noexcept {
    return master_semaphore->CurrentTick();
}

bool VKScheduler::IsFree(u64 tick) const noexcept {
    return master_semaphore->IsFree(tick);
}

void VKScheduler::Wait(u64 tick) {
    master_semaphore->Wait(tick);
}

void VKScheduler::Flush(VkSemaphore semaphore) {
    SubmitExecution(semaphore);
    AllocateNewContext();
}

void VKScheduler::Finish(VkSemaphore semaphore) {
    const u64 presubmit_tick = CurrentTick();
    SubmitExecution(semaphore);
    Wait(presubmit_tick);
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

    const VkSemaphore timeline_semaphore = master_semaphore->Handle();
    const u32 num_signal_semaphores = semaphore ? 2U : 1U;

    const u64 signal_value = master_semaphore->CurrentTick();
    const u64 wait_value = signal_value - 1;
    const VkPipelineStageFlags wait_stage_mask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    master_semaphore->NextTick();

    const std::array signal_values{signal_value, u64(0)};
    const std::array signal_semaphores{timeline_semaphore, semaphore};

    const VkTimelineSemaphoreSubmitInfoKHR timeline_si{
        .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreValueCount = 1,
        .pWaitSemaphoreValues = &wait_value,
        .signalSemaphoreValueCount = num_signal_semaphores,
        .pSignalSemaphoreValues = signal_values.data(),
    };
    const VkSubmitInfo submit_info{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = &timeline_si,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &timeline_semaphore,
        .pWaitDstStageMask = &wait_stage_mask,
        .commandBufferCount = 1,
        .pCommandBuffers = current_cmdbuf.address(),
        .signalSemaphoreCount = num_signal_semaphores,
        .pSignalSemaphores = signal_semaphores.data(),
    };
    switch (const VkResult result = device.GetGraphicsQueue().Submit(submit_info)) {
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
    std::unique_lock lock{mutex};

    current_cmdbuf = vk::CommandBuffer(command_pool->Commit(), device.GetDispatchLoader());
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
