// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <condition_variable>
#include <memory>
#include <stack>
#include <thread>
#include <utility>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class StateTracker;
class VKDevice;
class VKFence;
class VKQueryCache;
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
    explicit VKScheduler(const VKDevice& device, VKResourceManager& resource_manager,
                         StateTracker& state_tracker);
    ~VKScheduler();

    /// Sends the current execution context to the GPU.
    void Flush(bool release_fence = true, VkSemaphore semaphore = nullptr);

    /// Sends the current execution context to the GPU and waits for it to complete.
    void Finish(bool release_fence = true, VkSemaphore semaphore = nullptr);

    /// Waits for the worker thread to finish executing everything. After this function returns it's
    /// safe to touch worker resources.
    void WaitWorker();

    /// Sends currently recorded work to the worker thread.
    void DispatchWork();

    /// Requests to begin a renderpass.
    void RequestRenderpass(VkRenderPass renderpass, VkFramebuffer framebuffer,
                           VkExtent2D render_area);

    /// Requests the current executino context to be able to execute operations only allowed outside
    /// of a renderpass.
    void RequestOutsideRenderPassOperationContext();

    /// Binds a pipeline to the current execution context.
    void BindGraphicsPipeline(VkPipeline pipeline);

    /// Assigns the query cache.
    void SetQueryCache(VKQueryCache& query_cache_) {
        query_cache = &query_cache_;
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

    /// Returns the current command buffer tick.
    u64 Ticks() const {
        return ticks;
    }

private:
    class Command {
    public:
        virtual ~Command() = default;

        virtual void Execute(vk::CommandBuffer cmdbuf) const = 0;

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

        void Execute(vk::CommandBuffer cmdbuf) const override {
            command(cmdbuf);
        }

    private:
        T command;
    };

    class CommandChunk final {
    public:
        void ExecuteAll(vk::CommandBuffer cmdbuf);

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

    void SubmitExecution(VkSemaphore semaphore);

    void AllocateNewContext();

    void InvalidateState();

    void EndPendingOperations();

    void EndRenderPass();

    void AcquireNewChunk();

    const VKDevice& device;
    VKResourceManager& resource_manager;
    StateTracker& state_tracker;

    VKQueryCache* query_cache = nullptr;

    vk::CommandBuffer current_cmdbuf;
    VKFence* current_fence = nullptr;
    VKFence* next_fence = nullptr;

    struct State {
        VkRenderPass renderpass = nullptr;
        VkFramebuffer framebuffer = nullptr;
        VkExtent2D render_area = {0, 0};
        VkPipeline graphics_pipeline = nullptr;
    } state;

    std::unique_ptr<CommandChunk> chunk;
    std::thread worker_thread;

    Common::SPSCQueue<std::unique_ptr<CommandChunk>> chunk_queue;
    Common::SPSCQueue<std::unique_ptr<CommandChunk>> chunk_reserve;
    std::mutex mutex;
    std::condition_variable cv;
    std::atomic<u64> ticks = 0;
    bool quit = false;
};

} // namespace Vulkan
