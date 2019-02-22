// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKDevice;
class VKExecutionContext;
class VKFence;
class VKResourceManager;

/// The scheduler abstracts command buffer and fence management with an interface that's able to do
/// OpenGL-like operations on Vulkan command buffers.
class VKScheduler {
public:
    explicit VKScheduler(const VKDevice& device, VKResourceManager& resource_manager);
    ~VKScheduler();

    /// Gets the current execution context.
    [[nodiscard]] VKExecutionContext GetExecutionContext() const;

    /// Sends the current execution context to the GPU. It invalidates the current execution context
    /// and returns a new one.
    VKExecutionContext Flush(vk::Semaphore semaphore = nullptr);

    /// Sends the current execution context to the GPU and waits for it to complete. It invalidates
    /// the current execution context and returns a new one.
    VKExecutionContext Finish(vk::Semaphore semaphore = nullptr);

private:
    void SubmitExecution(vk::Semaphore semaphore);

    void AllocateNewContext();

    const VKDevice& device;
    VKResourceManager& resource_manager;
    vk::CommandBuffer current_cmdbuf;
    VKFence* current_fence = nullptr;
    VKFence* next_fence = nullptr;
};

class VKExecutionContext {
    friend class VKScheduler;

public:
    VKExecutionContext() = default;

    VKFence& GetFence() const {
        return *fence;
    }

    vk::CommandBuffer GetCommandBuffer() const {
        return cmdbuf;
    }

private:
    explicit VKExecutionContext(VKFence* fence, vk::CommandBuffer cmdbuf)
        : fence{fence}, cmdbuf{cmdbuf} {}

    VKFence* fence{};
    vk::CommandBuffer cmdbuf;
};

} // namespace Vulkan
