// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "video_core/query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace Vulkan {

class CachedQuery;
class HostCounter;
class VKDevice;
class VKQueryCache;
class VKScheduler;

using CounterStream = VideoCommon::CounterStreamBase<VKQueryCache, HostCounter>;

class QueryPool final : public VKFencedPool {
public:
    explicit QueryPool();
    ~QueryPool() override;

    void Initialize(const VKDevice& device, VideoCore::QueryType type);

    std::pair<VkQueryPool, u32> Commit(VKFence& fence);

    void Reserve(std::pair<VkQueryPool, u32> query);

protected:
    void Allocate(std::size_t begin, std::size_t end) override;

private:
    static constexpr std::size_t GROW_STEP = 512;

    const VKDevice* device = nullptr;
    VideoCore::QueryType type = {};

    std::vector<vk::QueryPool> pools;
    std::vector<bool> usage;
};

class VKQueryCache final
    : public VideoCommon::QueryCacheBase<VKQueryCache, CachedQuery, CounterStream, HostCounter,
                                         QueryPool> {
public:
    explicit VKQueryCache(VideoCore::RasterizerInterface& rasterizer,
                          Tegra::Engines::Maxwell3D& maxwell3d, Tegra::MemoryManager& gpu_memory,
                          const VKDevice& device, VKScheduler& scheduler);
    ~VKQueryCache();

    std::pair<VkQueryPool, u32> AllocateQuery(VideoCore::QueryType type);

    void Reserve(VideoCore::QueryType type, std::pair<VkQueryPool, u32> query);

    const VKDevice& Device() const noexcept {
        return device;
    }

    VKScheduler& Scheduler() const noexcept {
        return scheduler;
    }

private:
    const VKDevice& device;
    VKScheduler& scheduler;
};

class HostCounter final : public VideoCommon::HostCounterBase<VKQueryCache, HostCounter> {
public:
    explicit HostCounter(VKQueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type);
    ~HostCounter();

    void EndQuery();

private:
    u64 BlockingQuery() const override;

    VKQueryCache& cache;
    const VideoCore::QueryType type;
    const std::pair<VkQueryPool, u32> query;
    const u64 ticks;
};

class CachedQuery : public VideoCommon::CachedQueryBase<HostCounter> {
public:
    explicit CachedQuery(VKQueryCache&, VideoCore::QueryType, VAddr cpu_addr, u8* host_ptr)
        : VideoCommon::CachedQueryBase<HostCounter>{cpu_addr, host_ptr} {}
};

} // namespace Vulkan
