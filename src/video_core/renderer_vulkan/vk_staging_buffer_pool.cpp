// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/bit_util.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

VKStagingBufferPool::StagingBuffer::StagingBuffer(std::unique_ptr<VKBuffer> buffer_)
    : buffer{std::move(buffer_)} {}

VKStagingBufferPool::VKStagingBufferPool(const Device& device_, VKMemoryManager& memory_manager_,
                                         VKScheduler& scheduler_)
    : device{device_}, memory_manager{memory_manager_}, scheduler{scheduler_} {}

VKStagingBufferPool::~VKStagingBufferPool() = default;

VKBuffer& VKStagingBufferPool::GetUnusedBuffer(std::size_t size, bool host_visible) {
    if (const auto buffer = TryGetReservedBuffer(size, host_visible)) {
        return *buffer;
    }
    return CreateStagingBuffer(size, host_visible);
}

void VKStagingBufferPool::TickFrame() {
    current_delete_level = (current_delete_level + 1) % NumLevels;

    ReleaseCache(true);
    ReleaseCache(false);
}

VKBuffer* VKStagingBufferPool::TryGetReservedBuffer(std::size_t size, bool host_visible) {
    for (StagingBuffer& entry : GetCache(host_visible)[Common::Log2Ceil64(size)].entries) {
        if (!scheduler.IsFree(entry.tick)) {
            continue;
        }
        entry.tick = scheduler.CurrentTick();
        return &*entry.buffer;
    }
    return nullptr;
}

VKBuffer& VKStagingBufferPool::CreateStagingBuffer(std::size_t size, bool host_visible) {
    const u32 log2 = Common::Log2Ceil64(size);

    auto buffer = std::make_unique<VKBuffer>();
    buffer->handle = device.GetLogical().CreateBuffer({
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
    buffer->commit = memory_manager.Commit(buffer->handle, host_visible);

    std::vector<StagingBuffer>& entries = GetCache(host_visible)[log2].entries;
    StagingBuffer& entry = entries.emplace_back(std::move(buffer));
    entry.tick = scheduler.CurrentTick();
    return *entry.buffer;
}

VKStagingBufferPool::StagingBuffersCache& VKStagingBufferPool::GetCache(bool host_visible) {
    return host_visible ? host_staging_buffers : device_staging_buffers;
}

void VKStagingBufferPool::ReleaseCache(bool host_visible) {
    auto& cache = GetCache(host_visible);
    const u64 size = ReleaseLevel(cache, current_delete_level);
    if (size == 0) {
        return;
    }
}

u64 VKStagingBufferPool::ReleaseLevel(StagingBuffersCache& cache, std::size_t log2) {
    static constexpr std::size_t deletions_per_tick = 16;

    auto& staging = cache[log2];
    auto& entries = staging.entries;
    const std::size_t old_size = entries.size();

    const auto is_deleteable = [this](const StagingBuffer& entry) {
        return scheduler.IsFree(entry.tick);
    };
    const std::size_t begin_offset = staging.delete_index;
    const std::size_t end_offset = std::min(begin_offset + deletions_per_tick, old_size);
    const auto begin = std::begin(entries) + begin_offset;
    const auto end = std::begin(entries) + end_offset;
    entries.erase(std::remove_if(begin, end, is_deleteable), end);

    const std::size_t new_size = entries.size();
    staging.delete_index += deletions_per_tick;
    if (staging.delete_index >= new_size) {
        staging.delete_index = 0;
    }

    return (1ULL << log2) * (old_size - new_size);
}

} // namespace Vulkan
