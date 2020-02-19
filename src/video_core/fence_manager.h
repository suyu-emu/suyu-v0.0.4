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

    constexpr GPUVAddr GetAddress() const {
        return address;
    }

    constexpr u32 GetPayload() const {
        return payload;
    }

    constexpr bool IsSemaphore() const {
        return is_semaphore;
    }

private:
    GPUVAddr address;
    u32 payload;
    bool is_semaphore;

protected:
    bool is_stubbed;
};

template <typename TFence, typename TTextureCache, typename TTBufferCache>
class FenceManager {
public:
    void SignalSemaphore(GPUVAddr addr, u32 value) {
        TryReleasePendingFences();
        bool should_flush = texture_cache.HasUncommitedFlushes();
        should_flush |= buffer_cache.HasUncommitedFlushes();
        texture_cache.CommitAsyncFlushes();
        buffer_cache.CommitAsyncFlushes();
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
        bool should_flush = texture_cache.HasUncommitedFlushes();
        should_flush |= buffer_cache.HasUncommitedFlushes();
        texture_cache.CommitAsyncFlushes();
        buffer_cache.CommitAsyncFlushes();
        TFence new_fence = CreateFence(value, !should_flush);
        fences.push(new_fence);
        QueueFence(new_fence);
        if (should_flush) {
            rasterizer.FlushCommands();
        }
        rasterizer.SyncGuestHost();
    }

    void WaitPendingFences() {
        while (!fences.empty()) {
            TFence& current_fence = fences.front();
            bool should_wait = texture_cache.ShouldWaitAsyncFlushes();
            should_wait |= buffer_cache.ShouldWaitAsyncFlushes();
            if (should_wait) {
                WaitFence(current_fence);
            }
            texture_cache.PopAsyncFlushes();
            buffer_cache.PopAsyncFlushes();
            auto& gpu{system.GPU()};
            if (current_fence->IsSemaphore()) {
                auto& memory_manager{gpu.MemoryManager()};
                memory_manager.Write<u32>(current_fence->GetAddress(), current_fence->GetPayload());
            } else {
                gpu.IncrementSyncPoint(current_fence->GetPayload());
            }
            fences.pop();
        }
    }

protected:
    FenceManager(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                 TTextureCache& texture_cache, TTBufferCache& buffer_cache)
        : system{system}, rasterizer{rasterizer}, texture_cache{texture_cache}, buffer_cache{
                                                                                    buffer_cache} {}

    virtual TFence CreateFence(u32 value, bool is_stubbed) = 0;
    virtual TFence CreateFence(GPUVAddr addr, u32 value, bool is_stubbed) = 0;
    virtual void QueueFence(TFence& fence) = 0;
    virtual bool IsFenceSignaled(TFence& fence) = 0;
    virtual void WaitFence(TFence& fence) = 0;

    Core::System& system;
    VideoCore::RasterizerInterface& rasterizer;
    TTextureCache& texture_cache;
    TTBufferCache& buffer_cache;

private:
    void TryReleasePendingFences() {
        while (!fences.empty()) {
            TFence& current_fence = fences.front();
            bool should_wait = texture_cache.ShouldWaitAsyncFlushes();
            should_wait |= buffer_cache.ShouldWaitAsyncFlushes();
            if (should_wait && !IsFenceSignaled(current_fence)) {
                return;
            }
            texture_cache.PopAsyncFlushes();
            buffer_cache.PopAsyncFlushes();
            auto& gpu{system.GPU()};
            if (current_fence->IsSemaphore()) {
                auto& memory_manager{gpu.MemoryManager()};
                memory_manager.Write<u32>(current_fence->GetAddress(), current_fence->GetPayload());
            } else {
                gpu.IncrementSyncPoint(current_fence->GetPayload());
            }
            fences.pop();
        }
    }

    std::queue<TFence> fences;
};

} // namespace VideoCommon
