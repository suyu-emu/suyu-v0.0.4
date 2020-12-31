// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "common/bit_util.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan {

StagingBufferPool::StagingBufferPool(const Device& device_, VKMemoryManager& memory_manager_,
                                     VKScheduler& scheduler_)
    : device{device_}, memory_manager{memory_manager_}, scheduler{scheduler_} {}

StagingBufferPool::~StagingBufferPool() = default;

StagingBufferRef StagingBufferPool::Request(size_t size, bool host_visible) {
    if (const std::optional<StagingBufferRef> ref = TryGetReservedBuffer(size, host_visible)) {
        return *ref;
    }
    return CreateStagingBuffer(size, host_visible);
}

void StagingBufferPool::TickFrame() {
    current_delete_level = (current_delete_level + 1) % NUM_LEVELS;

    ReleaseCache(true);
    ReleaseCache(false);
}

std::optional<StagingBufferRef> StagingBufferPool::TryGetReservedBuffer(size_t size,
                                                                        bool host_visible) {
    StagingBuffers& cache_level = GetCache(host_visible)[Common::Log2Ceil64(size)];

    const auto is_free = [this](const StagingBuffer& entry) {
        return scheduler.IsFree(entry.tick);
    };
    auto& entries = cache_level.entries;
    const auto hint_it = entries.begin() + cache_level.iterate_index;
    auto it = std::find_if(entries.begin() + cache_level.iterate_index, entries.end(), is_free);
    if (it == entries.end()) {
        it = std::find_if(entries.begin(), hint_it, is_free);
        if (it == hint_it) {
            return std::nullopt;
        }
    }
    cache_level.iterate_index = std::distance(entries.begin(), it) + 1;
    it->tick = scheduler.CurrentTick();
    return it->Ref();
}

StagingBufferRef StagingBufferPool::CreateStagingBuffer(size_t size, bool host_visible) {
    const u32 log2 = Common::Log2Ceil64(size);
    vk::Buffer buffer = device.GetLogical().CreateBuffer({
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = 1ULL << log2,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
    if (device.HasDebuggingToolAttached()) {
        ++buffer_index;
        buffer.SetObjectNameEXT(fmt::format("Staging Buffer {}", buffer_index).c_str());
    }
    MemoryCommit commit = memory_manager.Commit(buffer, host_visible);
    const std::span<u8> mapped_span = host_visible ? commit.Map() : std::span<u8>{};

    StagingBuffer& entry = GetCache(host_visible)[log2].entries.emplace_back(StagingBuffer{
        .buffer = std::move(buffer),
        .commit = std::move(commit),
        .mapped_span = mapped_span,
        .tick = scheduler.CurrentTick(),
    });
    return entry.Ref();
}

StagingBufferPool::StagingBuffersCache& StagingBufferPool::GetCache(bool host_visible) {
    return host_visible ? host_staging_buffers : device_staging_buffers;
}

void StagingBufferPool::ReleaseCache(bool host_visible) {
    ReleaseLevel(GetCache(host_visible), current_delete_level);
}

void StagingBufferPool::ReleaseLevel(StagingBuffersCache& cache, size_t log2) {
    constexpr size_t deletions_per_tick = 16;
    auto& staging = cache[log2];
    auto& entries = staging.entries;
    const size_t old_size = entries.size();

    const auto is_deleteable = [this](const StagingBuffer& entry) {
        return scheduler.IsFree(entry.tick);
    };
    const size_t begin_offset = staging.delete_index;
    const size_t end_offset = std::min(begin_offset + deletions_per_tick, old_size);
    const auto begin = entries.begin() + begin_offset;
    const auto end = entries.begin() + end_offset;
    entries.erase(std::remove_if(begin, end, is_deleteable), end);

    const size_t new_size = entries.size();
    staging.delete_index += deletions_per_tick;
    if (staging.delete_index >= new_size) {
        staging.delete_index = 0;
    }
    if (staging.iterate_index > new_size) {
        staging.iterate_index = 0;
    }
}

} // namespace Vulkan
