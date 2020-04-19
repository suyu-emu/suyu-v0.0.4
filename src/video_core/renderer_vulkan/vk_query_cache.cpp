// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <utility>
#include <vector>

#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

namespace {

constexpr std::array QUERY_TARGETS = {VK_QUERY_TYPE_OCCLUSION};

constexpr VkQueryType GetTarget(VideoCore::QueryType type) {
    return QUERY_TARGETS[static_cast<std::size_t>(type)];
}

} // Anonymous namespace

QueryPool::QueryPool() : VKFencedPool{GROW_STEP} {}

QueryPool::~QueryPool() = default;

void QueryPool::Initialize(const VKDevice& device_, VideoCore::QueryType type_) {
    device = &device_;
    type = type_;
}

std::pair<VkQueryPool, u32> QueryPool::Commit(VKFence& fence) {
    std::size_t index;
    do {
        index = CommitResource(fence);
    } while (usage[index]);
    usage[index] = true;

    return {*pools[index / GROW_STEP], static_cast<u32>(index % GROW_STEP)};
}

void QueryPool::Allocate(std::size_t begin, std::size_t end) {
    usage.resize(end);

    VkQueryPoolCreateInfo query_pool_ci;
    query_pool_ci.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_ci.pNext = nullptr;
    query_pool_ci.flags = 0;
    query_pool_ci.queryType = GetTarget(type);
    query_pool_ci.queryCount = static_cast<u32>(end - begin);
    query_pool_ci.pipelineStatistics = 0;
    pools.push_back(device->GetLogical().CreateQueryPool(query_pool_ci));
}

void QueryPool::Reserve(std::pair<VkQueryPool, u32> query) {
    const auto it =
        std::find_if(pools.begin(), pools.end(), [query_pool = query.first](vk::QueryPool& pool) {
            return query_pool == *pool;
        });
    ASSERT(it != std::end(pools));

    const std::ptrdiff_t pool_index = std::distance(std::begin(pools), it);
    usage[pool_index * GROW_STEP + static_cast<std::ptrdiff_t>(query.second)] = false;
}

VKQueryCache::VKQueryCache(Core::System& system, VideoCore::RasterizerInterface& rasterizer,
                           const VKDevice& device, VKScheduler& scheduler)
    : VideoCommon::QueryCacheBase<VKQueryCache, CachedQuery, CounterStream, HostCounter,
                                  QueryPool>{system, rasterizer},
      device{device}, scheduler{scheduler} {
    for (std::size_t i = 0; i < static_cast<std::size_t>(VideoCore::NumQueryTypes); ++i) {
        query_pools[i].Initialize(device, static_cast<VideoCore::QueryType>(i));
    }
}

VKQueryCache::~VKQueryCache() = default;

std::pair<VkQueryPool, u32> VKQueryCache::AllocateQuery(VideoCore::QueryType type) {
    return query_pools[static_cast<std::size_t>(type)].Commit(scheduler.GetFence());
}

void VKQueryCache::Reserve(VideoCore::QueryType type, std::pair<VkQueryPool, u32> query) {
    query_pools[static_cast<std::size_t>(type)].Reserve(query);
}

HostCounter::HostCounter(VKQueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type)
    : VideoCommon::HostCounterBase<VKQueryCache, HostCounter>{std::move(dependency)}, cache{cache},
      type{type}, query{cache.AllocateQuery(type)}, ticks{cache.Scheduler().Ticks()} {
    const vk::Device* logical = &cache.Device().GetLogical();
    cache.Scheduler().Record([logical, query = query](vk::CommandBuffer cmdbuf) {
        logical->ResetQueryPoolEXT(query.first, query.second, 1);
        cmdbuf.BeginQuery(query.first, query.second, VK_QUERY_CONTROL_PRECISE_BIT);
    });
}

HostCounter::~HostCounter() {
    cache.Reserve(type, query);
}

void HostCounter::EndQuery() {
    cache.Scheduler().Record(
        [query = query](vk::CommandBuffer cmdbuf) { cmdbuf.EndQuery(query.first, query.second); });
}

u64 HostCounter::BlockingQuery() const {
    if (ticks >= cache.Scheduler().Ticks()) {
        cache.Scheduler().Flush();
    }
    u64 data;
    const VkResult result = cache.Device().GetLogical().GetQueryResults(
        query.first, query.second, 1, sizeof(data), &data, sizeof(data),
        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    switch (result) {
    case VK_SUCCESS:
        return data;
    case VK_ERROR_DEVICE_LOST:
        cache.Device().ReportLoss();
        [[fallthrough]];
    default:
        throw vk::Exception(result);
    }
}

} // namespace Vulkan
