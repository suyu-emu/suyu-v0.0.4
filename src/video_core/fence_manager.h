// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <queue>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/core.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace VideoCommon {

class FenceBase {
public:
    FenceBase(u32 payload, bool is_stubbed)
        : address{}, payload{payload}, is_semaphore{false}, is_stubbed{is_stubbed} {}

    FenceBase(GPUVAddr address, u32 payload, bool is_stubbed)
        : address{address}, payload{payload}, is_semaphore{true}, is_stubbed{is_stubbed} {}

    GPUVAddr GetAddress() const {
        return address;
    }

    u32 GetPayload() const {
        return payload;
    }

    bool IsSemaphore() const {
        return is_semaphore;
    }

private:
    GPUVAddr address;
    u32 payload;
    bool is_semaphore;

protected:
    bool is_stubbed;
};

template <typename TFence, typename TTextureCache, typename TTBufferCache, typename TQueryCache>
class FenceManager {
public:
    void SignalSemaphore(GPUVAddr addr, u32 value) {
        TryReleasePendingFences();
        const bool should_flush = ShouldFlush();
        CommitAsyncFlushes();
        TFence new_fence = CreateFence(addr, value, !should_flush);
        fences.push(new_fence);
        QueueFence(new_fence);
        if (should_flush) {
            rasterizer.FlushCommands();
        }
        rasterizer.SyncGuestHost();
    }

    void SignalSyncPoint(u32 value) {
        TryReleasePendingFences();
        const bool should_flush = ShouldFlush();
        CommitAsyncFlushes();
        TFence new_fence = CreateFence(value, !should_flush);
        fences.push(new_fence);
        QueueFence(new_fence);
        if (should_flush) {
            rasterizer.FlushCommands();
        }
        rasterizer.SyncGuestHost();
    }

    void WaitPendingFences() {
        auto& gpu{system.GPU()};
        auto& memory_manager{gpu.MemoryManager()};
        while (!fences.empty()) {
            TFence& current_fence = fences.front();
            if (ShouldWait()) {
                WaitFence(current_fence);
            }
            PopAsyncFlushes();
            if (current_fence->IsSemaphore()) {
                memory_manager.Write<u32>(current_fence->GetAddress(), current_fence->GetPayload());
            } else {
                gpu.IncrementSyncPoint(current_fence->GetPayload());
            }
            fences.pop();
        }
    }

protected:
    FenceManager(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                 TTextureCache& texture_cache, TTBufferCache& buffer_cache,
                 TQueryCache& query_cache)
        : system{system}, rasterizer{rasterizer}, texture_cache{texture_cache},
          buffer_cache{buffer_cache}, query_cache{query_cache} {}

    virtual ~FenceManager() {}

    /// Creates a Sync Point Fence Interface, does not create a backend fence if 'is_stubbed' is
    /// true
    virtual TFence CreateFence(u32 value, bool is_stubbed) = 0;
    /// Creates a Semaphore Fence Interface, does not create a backend fence if 'is_stubbed' is true
    virtual TFence CreateFence(GPUVAddr addr, u32 value, bool is_stubbed) = 0;
    /// Queues a fence into the backend if the fence isn't stubbed.
    virtual void QueueFence(TFence& fence) = 0;
    /// Notifies that the backend fence has been signaled/reached in host GPU.
    virtual bool IsFenceSignaled(TFence& fence) const = 0;
    /// Waits until a fence has been signalled by the host GPU.
    virtual void WaitFence(TFence& fence) = 0;

    Core::System& system;
    VideoCore::RasterizerInterface& rasterizer;
    TTextureCache& texture_cache;
    TTBufferCache& buffer_cache;
    TQueryCache& query_cache;

private:
    void TryReleasePendingFences() {
        auto& gpu{system.GPU()};
        auto& memory_manager{gpu.MemoryManager()};
        while (!fences.empty()) {
            TFence& current_fence = fences.front();
            if (ShouldWait() && !IsFenceSignaled(current_fence)) {
                return;
            }
            PopAsyncFlushes();
            if (current_fence->IsSemaphore()) {
                memory_manager.Write<u32>(current_fence->GetAddress(), current_fence->GetPayload());
            } else {
                gpu.IncrementSyncPoint(current_fence->GetPayload());
            }
            fences.pop();
        }
    }

    bool ShouldWait() const {
        return texture_cache.ShouldWaitAsyncFlushes() || buffer_cache.ShouldWaitAsyncFlushes() ||
               query_cache.ShouldWaitAsyncFlushes();
    }

    bool ShouldFlush() const {
        return texture_cache.HasUncommittedFlushes() || buffer_cache.HasUncommittedFlushes() ||
               query_cache.HasUncommittedFlushes();
    }

    void PopAsyncFlushes() {
        texture_cache.PopAsyncFlushes();
        buffer_cache.PopAsyncFlushes();
        query_cache.PopAsyncFlushes();
    }

    void CommitAsyncFlushes() {
        texture_cache.CommitAsyncFlushes();
        buffer_cache.CommitAsyncFlushes();
        query_cache.CommitAsyncFlushes();
    }

    std::queue<TFence> fences;
};

} // namespace VideoCommon
