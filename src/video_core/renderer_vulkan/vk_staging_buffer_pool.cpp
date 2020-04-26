// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/bit_util.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

VKStagingBufferPool::StagingBuffer::StagingBuffer(std::unique_ptr<VKBuffer> buffer, VKFence& fence,
                                                  u64 last_epoch)
    : buffer{std::move(buffer)}, watch{fence}, last_epoch{last_epoch} {}

VKStagingBufferPool::StagingBuffer::StagingBuffer(StagingBuffer&& rhs) noexcept {
    buffer = std::move(rhs.buffer);
    watch = std::move(rhs.watch);
    last_epoch = rhs.last_epoch;
}

VKStagingBufferPool::StagingBuffer::~StagingBuffer() = default;

VKStagingBufferPool::StagingBuffer& VKStagingBufferPool::StagingBuffer::operator=(
    StagingBuffer&& rhs) noexcept {
    buffer = std::move(rhs.buffer);
    watch = std::move(rhs.watch);
    last_epoch = rhs.last_epoch;
    return *this;
}

VKStagingBufferPool::VKStagingBufferPool(const VKDevice& device, VKMemoryManager& memory_manager,
                                         VKScheduler& scheduler)
    : device{device}, memory_manager{memory_manager}, scheduler{scheduler},
      is_device_integrated{device.IsIntegrated()} {}

VKStagingBufferPool::~VKStagingBufferPool() = default;

VKBuffer& VKStagingBufferPool::GetUnusedBuffer(std::size_t size, bool host_visible) {
    if (const auto buffer = TryGetReservedBuffer(size, host_visible)) {
        return *buffer;
    }
    return CreateStagingBuffer(size, host_visible);
}

void VKStagingBufferPool::TickFrame() {
    ++epoch;
    current_delete_level = (current_delete_level + 1) % NumLevels;

    ReleaseCache(true);
    if (!is_device_integrated) {
        ReleaseCache(false);
    }
}

VKBuffer* VKStagingBufferPool::TryGetReservedBuffer(std::size_t size, bool host_visible) {
    for (auto& entry : GetCache(host_visible)[Common::Log2Ceil64(size)].entries) {
        if (entry.watch.TryWatch(scheduler.GetFence())) {
            entry.last_epoch = epoch;
            return &*entry.buffer;
        }
    }
    return nullptr;
}

VKBuffer& VKStagingBufferPool::CreateStagingBuffer(std::size_t size, bool host_visible) {
    const u32 log2 = Common::Log2Ceil64(size);

    VkBufferCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.size = 1ULL << log2;
    ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
               VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
               VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount = 0;
    ci.pQueueFamilyIndices = nullptr;

    auto buffer = std::make_unique<VKBuffer>();
    buffer->handle = device.GetLogical().CreateBuffer(ci);
    buffer->commit = memory_manager.Commit(buffer->handle, host_visible);

    auto& entries = GetCache(host_visible)[log2].entries;
    return *entries.emplace_back(std::move(buffer), scheduler.GetFence(), epoch).buffer;
}

VKStagingBufferPool::StagingBuffersCache& VKStagingBufferPool::GetCache(bool host_visible) {
    return is_device_integrated || host_visible ? host_staging_buffers : device_staging_buffers;
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

    const auto is_deleteable = [this](const auto& entry) {
        static constexpr u64 epochs_to_destroy = 180;
        return entry.last_epoch + epochs_to_destroy < epoch && !entry.watch.IsUsed();
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
