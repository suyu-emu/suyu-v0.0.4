// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/microprofile.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

MICROPROFILE_DECLARE(Vulkan_WaitForWorker);

void VKScheduler::CommandChunk::ExecuteAll(vk::CommandBuffer cmdbuf,
                                           const vk::DispatchLoaderDynamic& dld) {
    auto command = first;
    while (command != nullptr) {
        auto next = command->GetNext();
        command->Execute(cmdbuf, dld);
        command->~Command();
        command = next;
    }

    command_offset = 0;
    first = nullptr;
    last = nullptr;
}

VKScheduler::VKScheduler(const VKDevice& device, VKResourceManager& resource_manager)
    : device{device}, resource_manager{resource_manager}, next_fence{
                                                              &resource_manager.CommitFence()} {
    AcquireNewChunk();
    AllocateNewContext();
    worker_thread = std::thread(&VKScheduler::WorkerThread, this);
}

VKScheduler::~VKScheduler() {
    quit = true;
    cv.notify_all();
    worker_thread.join();
}

void VKScheduler::Flush(bool release_fence, vk::Semaphore semaphore) {
    SubmitExecution(semaphore);
    if (release_fence) {
        current_fence->Release();
    }
    AllocateNewContext();
}

void VKScheduler::Finish(bool release_fence, vk::Semaphore semaphore) {
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

void VKScheduler::RequestRenderpass(const vk::RenderPassBeginInfo& renderpass_bi) {
    if (state.renderpass && renderpass_bi == *state.renderpass) {
        return;
    }
    const bool end_renderpass = state.renderpass.has_value();
    state.renderpass = renderpass_bi;
    Record([renderpass_bi, end_renderpass](auto cmdbuf, auto& dld) {
        if (end_renderpass) {
            cmdbuf.endRenderPass(dld);
        }
        cmdbuf.beginRenderPass(renderpass_bi, vk::SubpassContents::eInline, dld);
    });
}

void VKScheduler::RequestOutsideRenderPassOperationContext() {
    EndRenderPass();
}

void VKScheduler::BindGraphicsPipeline(vk::Pipeline pipeline) {
    if (state.graphics_pipeline == pipeline) {
        return;
    }
    state.graphics_pipeline = pipeline;
    Record([pipeline](auto cmdbuf, auto& dld) {
        cmdbuf.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline, dld);
    });
}

void VKScheduler::WorkerThread() {
    std::unique_lock lock{mutex};
    do {
        cv.wait(lock, [this] { return !chunk_queue.Empty() || quit; });
        if (quit) {
            continue;
        }
        auto extracted_chunk = std::move(chunk_queue.Front());
        chunk_queue.Pop();
        extracted_chunk->ExecuteAll(current_cmdbuf, device.GetDispatchLoader());
        chunk_reserve.Push(std::move(extracted_chunk));
    } while (!quit);
}

void VKScheduler::SubmitExecution(vk::Semaphore semaphore) {
    EndPendingOperations();
    InvalidateState();
    WaitWorker();

    std::unique_lock lock{mutex};

    const auto queue = device.GetGraphicsQueue();
    const auto& dld = device.GetDispatchLoader();
    current_cmdbuf.end(dld);

    const vk::SubmitInfo submit_info(0, nullptr, nullptr, 1, &current_cmdbuf, semaphore ? 1U : 0U,
                                     &semaphore);
    queue.submit({submit_info}, static_cast<vk::Fence>(*current_fence), dld);
}

void VKScheduler::AllocateNewContext() {
    ++ticks;

    std::unique_lock lock{mutex};
    current_fence = next_fence;
    next_fence = &resource_manager.CommitFence();

    current_cmdbuf = resource_manager.CommitCommandBuffer(*current_fence);
    current_cmdbuf.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit},
                         device.GetDispatchLoader());
    // Enable counters once again. These are disabled when a command buffer is finished.
    if (query_cache) {
        query_cache->UpdateCounters();
    }
}

void VKScheduler::InvalidateState() {
    state.graphics_pipeline = nullptr;
    state.viewports = false;
    state.scissors = false;
    state.depth_bias = false;
    state.blend_constants = false;
    state.depth_bounds = false;
    state.stencil_values = false;
}

void VKScheduler::EndPendingOperations() {
    query_cache->DisableStreams();
    EndRenderPass();
}

void VKScheduler::EndRenderPass() {
    if (!state.renderpass) {
        return;
    }
    state.renderpass = std::nullopt;
    Record([](auto cmdbuf, auto& dld) { cmdbuf.endRenderPass(dld); });
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
