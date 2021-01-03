// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <climits>
#include <vector>

#include "common/common_types.h"

#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class VKScheduler;

struct StagingBufferRef {
    VkBuffer buffer;
    std::span<u8> mapped_span;
};

class StagingBufferPool {
public:
    explicit StagingBufferPool(const Device& device, MemoryAllocator& memory_allocator,
                               VKScheduler& scheduler);
    ~StagingBufferPool();

    StagingBufferRef Request(size_t size, MemoryUsage usage);

    void TickFrame();

private:
    struct StagingBuffer {
        vk::Buffer buffer;
        MemoryCommit commit;
        std::span<u8> mapped_span;
        u64 tick = 0;

        StagingBufferRef Ref() const noexcept {
            return {
                .buffer = *buffer,
                .mapped_span = mapped_span,
            };
        }
    };

    struct StagingBuffers {
        std::vector<StagingBuffer> entries;
        size_t delete_index = 0;
        size_t iterate_index = 0;
    };

    static constexpr size_t NUM_LEVELS = sizeof(size_t) * CHAR_BIT;
    using StagingBuffersCache = std::array<StagingBuffers, NUM_LEVELS>;

    std::optional<StagingBufferRef> TryGetReservedBuffer(size_t size, MemoryUsage usage);

    StagingBufferRef CreateStagingBuffer(size_t size, MemoryUsage usage);

    StagingBuffersCache& GetCache(MemoryUsage usage);

    void ReleaseCache(MemoryUsage usage);

    void ReleaseLevel(StagingBuffersCache& cache, size_t log2);

    const Device& device;
    MemoryAllocator& memory_allocator;
    VKScheduler& scheduler;

    StagingBuffersCache device_local_cache;
    StagingBuffersCache upload_cache;
    StagingBuffersCache download_cache;

    size_t current_delete_level = 0;
    u64 buffer_index = 0;
};

} // namespace Vulkan
