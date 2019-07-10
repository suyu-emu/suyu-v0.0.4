// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKDevice;
class VKFence;
class VKResourceManager;

class VKFenceView {
public:
    VKFenceView() = default;
    VKFenceView(VKFence* const& fence) : fence{fence} {}

    VKFence* operator->() const noexcept {
        return fence;
    }

    operator VKFence&() const noexcept {
        return *fence;
    }

private:
    VKFence* const& fence;
};

class VKCommandBufferView {
public:
    VKCommandBufferView() = default;
    VKCommandBufferView(const vk::CommandBuffer& cmdbuf) : cmdbuf{cmdbuf} {}

    const vk::CommandBuffer* operator->() const noexcept {
        return &cmdbuf;
    }

    operator vk::CommandBuffer() const noexcept {
        return cmdbuf;
    }

private:
    const vk::CommandBuffer& cmdbuf;
};

/// The scheduler abstracts command buffer and fence management with an interface that's able to do
/// OpenGL-like operations on Vulkan command buffers.
class VKScheduler {
public:
    explicit VKScheduler(const VKDevice& device, VKResourceManager& resource_manager);
    ~VKScheduler();

    /// Gets a reference to the current fence.
    VKFenceView GetFence() const {
        return current_fence;
    }

    /// Gets a reference to the current command buffer.
    VKCommandBufferView GetCommandBuffer() const {
        return current_cmdbuf;
    }

    /// Sends the current execution context to the GPU.
    void Flush(bool release_fence = true, vk::Semaphore semaphore = nullptr);

    /// Sends the current execution context to the GPU and waits for it to complete.
    void Finish(bool release_fence = true, vk::Semaphore semaphore = nullptr);

private:
    void SubmitExecution(vk::Semaphore semaphore);

    void AllocateNewContext();

    const VKDevice& device;
    VKResourceManager& resource_manager;
    vk::CommandBuffer current_cmdbuf;
    VKFence* current_fence = nullptr;
    VKFence* next_fence = nullptr;
};

} // namespace Vulkan
