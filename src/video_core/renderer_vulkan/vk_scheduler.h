// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <condition_variable>
#include <memory>
#include <optional>
#include <stack>
#include <thread>
#include <utility>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"
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

/// The scheduler abstracts command buffer and fence management with an interface that's able to do
/// OpenGL-like operations on Vulkan command buffers.
class VKScheduler {
public:
    explicit VKScheduler(const VKDevice& device, VKResourceManager& resource_manager);
    ~VKScheduler();

    /// Sends the current execution context to the GPU.
    void Flush(bool release_fence = true, vk::Semaphore semaphore = nullptr);

    /// Sends the current execution context to the GPU and waits for it to complete.
    void Finish(bool release_fence = true, vk::Semaphore semaphore = nullptr);

    /// Waits for the worker thread to finish executing everything. After this function returns it's
    /// safe to touch worker resources.
    void WaitWorker();

    /// Sends currently recorded work to the worker thread.
    void DispatchWork();

    /// Requests to begin a renderpass.
    void RequestRenderpass(const vk::RenderPassBeginInfo& renderpass_bi);

    /// Requests the current executino context to be able to execute operations only allowed outside
    /// of a renderpass.
    void RequestOutsideRenderPassOperationContext();

    /// Binds a pipeline to the current execution context.
    void BindGraphicsPipeline(vk::Pipeline pipeline);

    /// Returns true when viewports have been set in the current command buffer.
    bool TouchViewports() {
        return std::exchange(state.viewports, true);
    }

    /// Returns true when scissors have been set in the current command buffer.
    bool TouchScissors() {
        return std::exchange(state.scissors, true);
    }

    /// Returns true when depth bias have been set in the current command buffer.
    bool TouchDepthBias() {
        return std::exchange(state.depth_bias, true);
    }

    /// Returns true when blend constants have been set in the current command buffer.
    bool TouchBlendConstants() {
        return std::exchange(state.blend_constants, true);
    }

    /// Returns true when depth bounds have been set in the current command buffer.
    bool TouchDepthBounds() {
        return std::exchange(state.depth_bounds, true);
    }

    /// Returns true when stencil values have been set in the current command buffer.
    bool TouchStencilValues() {
        return std::exchange(state.stencil_values, true);
    }

    /// Send work to a separate thread.
    template <typename T>
    void Record(T&& command) {
        if (chunk->Record(command)) {
            return;
        }
        DispatchWork();
        (void)chunk->Record(command);
    }

    /// Gets a reference to the current fence.
    VKFenceView GetFence() const {
        return current_fence;
    }

private:
    class Command {
    public:
        virtual ~Command() = default;

        virtual void Execute(vk::CommandBuffer cmdbuf,
                             const vk::DispatchLoaderDynamic& dld) const = 0;

        Command* GetNext() const {
            return next;
        }

        void SetNext(Command* next_) {
            next = next_;
        }

    private:
        Command* next = nullptr;
    };

    template <typename T>
    class TypedCommand final : public Command {
    public:
        explicit TypedCommand(T&& command) : command{std::move(command)} {}
        ~TypedCommand() override = default;

        TypedCommand(TypedCommand&&) = delete;
        TypedCommand& operator=(TypedCommand&&) = delete;

        void Execute(vk::CommandBuffer cmdbuf,
                     const vk::DispatchLoaderDynamic& dld) const override {
            command(cmdbuf, dld);
        }

    private:
        T command;
    };

    class CommandChunk final {
    public:
        void ExecuteAll(vk::CommandBuffer cmdbuf, const vk::DispatchLoaderDynamic& dld);

        template <typename T>
        bool Record(T& command) {
            using FuncType = TypedCommand<T>;
            static_assert(sizeof(FuncType) < sizeof(data), "Lambda is too large");

            if (command_offset > sizeof(data) - sizeof(FuncType)) {
                return false;
            }

            Command* current_last = last;

            last = new (data.data() + command_offset) FuncType(std::move(command));

            if (current_last) {
                current_last->SetNext(last);
            } else {
                first = last;
            }

            command_offset += sizeof(FuncType);
            return true;
        }

        bool Empty() const {
            return command_offset == 0;
        }

    private:
        Command* first = nullptr;
        Command* last = nullptr;

        std::size_t command_offset = 0;
        std::array<u8, 0x8000> data{};
    };

    void WorkerThread();

    void SubmitExecution(vk::Semaphore semaphore);

    void AllocateNewContext();

    void InvalidateState();

    void EndPendingOperations();

    void EndRenderPass();

    void AcquireNewChunk();

    const VKDevice& device;
    VKResourceManager& resource_manager;
    vk::CommandBuffer current_cmdbuf;
    VKFence* current_fence = nullptr;
    VKFence* next_fence = nullptr;

    struct State {
        std::optional<vk::RenderPassBeginInfo> renderpass;
        vk::Pipeline graphics_pipeline;
        bool viewports = false;
        bool scissors = false;
        bool depth_bias = false;
        bool blend_constants = false;
        bool depth_bounds = false;
        bool stencil_values = false;
    } state;

    std::unique_ptr<CommandChunk> chunk;
    std::thread worker_thread;

    Common::SPSCQueue<std::unique_ptr<CommandChunk>> chunk_queue;
    Common::SPSCQueue<std::unique_ptr<CommandChunk>> chunk_reserve;
    std::mutex mutex;
    std::condition_variable cv;
    bool quit = false;
};

} // namespace Vulkan
