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
    FenceBase(GPUVAddr address, u32 payload) : address{address}, payload{payload} {}

    constexpr GPUVAddr GetAddress() const {
        return address;
    }

    constexpr u32 GetPayload() const {
        return payload;
    }

private:
    GPUVAddr address;
    u32 payload;
};

template <typename TFence, typename TTextureCache, typename TTBufferCache>
class FenceManager {
public:
    void SignalFence(GPUVAddr addr, u32 value) {
        TryReleasePendingFences();
        TFence new_fence = CreateFence(addr, value);
        QueueFence(new_fence);
        fences.push(new_fence);
        texture_cache.CommitAsyncFlushes();
        buffer_cache.CommitAsyncFlushes();
        rasterizer.FlushCommands();
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
            auto& memory_manager{gpu.MemoryManager()};
            memory_manager.Write<u32>(current_fence->GetAddress(), current_fence->GetPayload());
            fences.pop();
        }
    }

protected:
    FenceManager(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                 TTextureCache& texture_cache, TTBufferCache& buffer_cache)
        : system{system}, rasterizer{rasterizer}, texture_cache{texture_cache}, buffer_cache{
                                                                                    buffer_cache} {}

    virtual TFence CreateFence(GPUVAddr addr, u32 value) = 0;
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
            auto& memory_manager{gpu.MemoryManager()};
            memory_manager.Write<u32>(current_fence->GetAddress(), current_fence->GetPayload());
            fences.pop();
        }
    }

    std::queue<TFence> fences;
};

} // namespace VideoCommon
