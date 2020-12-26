// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "video_core/fence_manager.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Core {
class System;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace Vulkan {

class Device;
class VKBufferCache;
class VKQueryCache;
class VKScheduler;

class InnerFence : public VideoCommon::FenceBase {
public:
    explicit InnerFence(const Device& device_, VKScheduler& scheduler_, u32 payload_,
                        bool is_stubbed_);
    explicit InnerFence(const Device& device_, VKScheduler& scheduler_, GPUVAddr address_,
                        u32 payload_, bool is_stubbed_);
    ~InnerFence();

    void Queue();

    bool IsSignaled() const;

    void Wait();

private:
    bool IsEventSignalled() const;

    const Device& device;
    VKScheduler& scheduler;
    vk::Event event;
    u64 ticks = 0;
};
using Fence = std::shared_ptr<InnerFence>;

using GenericFenceManager =
    VideoCommon::FenceManager<Fence, TextureCache, VKBufferCache, VKQueryCache>;

class VKFenceManager final : public GenericFenceManager {
public:
    explicit VKFenceManager(VideoCore::RasterizerInterface& rasterizer_, Tegra::GPU& gpu_,
                            Tegra::MemoryManager& memory_manager_, TextureCache& texture_cache_,
                            VKBufferCache& buffer_cache_, VKQueryCache& query_cache_,
                            const Device& device_, VKScheduler& scheduler_);

protected:
    Fence CreateFence(u32 value, bool is_stubbed) override;
    Fence CreateFence(GPUVAddr addr, u32 value, bool is_stubbed) override;
    void QueueFence(Fence& fence) override;
    bool IsFenceSignaled(Fence& fence) const override;
    void WaitFence(Fence& fence) override;

private:
    const Device& device;
    VKScheduler& scheduler;
};

} // namespace Vulkan
