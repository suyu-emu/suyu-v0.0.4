// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "video_core/fence_manager.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Core {
class System;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace Vulkan {

class VKBufferCache;
class VKDevice;
class VKQueryCache;
class VKScheduler;
class VKTextureCache;

class InnerFence : public VideoCommon::FenceBase {
public:
    explicit InnerFence(const VKDevice& device, VKScheduler& scheduler, u32 payload,
                        bool is_stubbed);
    explicit InnerFence(const VKDevice& device, VKScheduler& scheduler, GPUVAddr address,
                        u32 payload, bool is_stubbed);
    ~InnerFence();

    void Queue();

    bool IsSignaled() const;

    void Wait();

private:
    bool IsEventSignalled() const;

    const VKDevice& device;
    VKScheduler& scheduler;
    vk::Event event;
    u64 ticks = 0;
};
using Fence = std::shared_ptr<InnerFence>;

using GenericFenceManager =
    VideoCommon::FenceManager<Fence, VKTextureCache, VKBufferCache, VKQueryCache>;

class VKFenceManager final : public GenericFenceManager {
public:
    explicit VKFenceManager(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                            const VKDevice& device, VKScheduler& scheduler,
                            VKTextureCache& texture_cache, VKBufferCache& buffer_cache,
                            VKQueryCache& query_cache);

protected:
    Fence CreateFence(u32 value, bool is_stubbed) override;
    Fence CreateFence(GPUVAddr addr, u32 value, bool is_stubbed) override;
    void QueueFence(Fence& fence) override;
    bool IsFenceSignaled(Fence& fence) override;
    void WaitFence(Fence& fence) override;

private:
    const VKDevice& device;
    VKScheduler& scheduler;
};

} // namespace Vulkan
