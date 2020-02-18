// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_query_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"

namespace Vulkan {

namespace {

constexpr std::array QUERY_TARGETS = {vk::QueryType::eOcclusion};

constexpr vk::QueryType GetTarget(VideoCore::QueryType type) {
    return QUERY_TARGETS[static_cast<std::size_t>(type)];
}

} // Anonymous namespace

QueryPool::QueryPool() : VKFencedPool{GROW_STEP} {}

QueryPool::~QueryPool() = default;

void QueryPool::Initialize(const VKDevice& device_, VideoCore::QueryType type_) {
    device = &device_;
    type = type_;
}

std::pair<vk::QueryPool, std::uint32_t> QueryPool::Commit(VKFence& fence) {
    std::size_t index;
    do {
        index = CommitResource(fence);
    } while (usage[index]);
    usage[index] = true;

    return {*pools[index / GROW_STEP], static_cast<std::uint32_t>(index % GROW_STEP)};
}

void QueryPool::Allocate(std::size_t begin, std::size_t end) {
    usage.resize(end);

    const auto dev = device->GetLogical();
    const u32 size = static_cast<u32>(end - begin);
    const vk::QueryPoolCreateInfo query_pool_ci({}, GetTarget(type), size, {});
    pools.push_back(dev.createQueryPoolUnique(query_pool_ci, nullptr, device->GetDispatchLoader()));
}

void QueryPool::Reserve(std::pair<vk::QueryPool, std::uint32_t> query) {
    const auto it =
        std::find_if(std::begin(pools), std::end(pools),
                     [query_pool = query.first](auto& pool) { return query_pool == *pool; });
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

std::pair<vk::QueryPool, std::uint32_t> VKQueryCache::AllocateQuery(VideoCore::QueryType type) {
    return query_pools[static_cast<std::size_t>(type)].Commit(scheduler.GetFence());
}

void VKQueryCache::Reserve(VideoCore::QueryType type,
                           std::pair<vk::QueryPool, std::uint32_t> query) {
    query_pools[static_cast<std::size_t>(type)].Reserve(query);
}

HostCounter::HostCounter(VKQueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type)
    : VideoCommon::HostCounterBase<VKQueryCache, HostCounter>{std::move(dependency)}, cache{cache},
      type{type}, query{cache.AllocateQuery(type)}, ticks{cache.Scheduler().Ticks()} {
    const auto dev = cache.Device().GetLogical();
    cache.Scheduler().Record([dev, query = query](vk::CommandBuffer cmdbuf, auto& dld) {
        dev.resetQueryPoolEXT(query.first, query.second, 1, dld);
        cmdbuf.beginQuery(query.first, query.second, vk::QueryControlFlagBits::ePrecise, dld);
    });
}

HostCounter::~HostCounter() {
    cache.Reserve(type, query);
}

void HostCounter::EndQuery() {
    cache.Scheduler().Record([query = query](auto cmdbuf, auto& dld) {
        cmdbuf.endQuery(query.first, query.second, dld);
    });
}

u64 HostCounter::BlockingQuery() const {
    if (ticks >= cache.Scheduler().Ticks()) {
        cache.Scheduler().Flush();
    }

    const auto dev = cache.Device().GetLogical();
    const auto& dld = cache.Device().GetDispatchLoader();
    u64 value;
    dev.getQueryPoolResults(query.first, query.second, 1, sizeof(value), &value, sizeof(value),
                            vk::QueryResultFlagBits::e64 | vk::QueryResultFlagBits::eWait, dld);
    return value;
}

} // namespace Vulkan
