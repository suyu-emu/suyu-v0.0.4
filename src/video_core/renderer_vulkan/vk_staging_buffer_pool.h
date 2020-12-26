// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <climits>
#include <vector>

#include "common/common_types.h"

#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class VKScheduler;

struct VKBuffer final {
    vk::Buffer handle;
    VKMemoryCommit commit;
};

class VKStagingBufferPool final {
public:
    explicit VKStagingBufferPool(const Device& device, VKMemoryManager& memory_manager,
                                 VKScheduler& scheduler);
    ~VKStagingBufferPool();

    VKBuffer& GetUnusedBuffer(std::size_t size, bool host_visible);

    void TickFrame();

private:
    struct StagingBuffer final {
        explicit StagingBuffer(std::unique_ptr<VKBuffer> buffer);

        std::unique_ptr<VKBuffer> buffer;
        u64 tick = 0;
    };

    struct StagingBuffers final {
        std::vector<StagingBuffer> entries;
        std::size_t delete_index = 0;
    };

    static constexpr std::size_t NumLevels = sizeof(std::size_t) * CHAR_BIT;
    using StagingBuffersCache = std::array<StagingBuffers, NumLevels>;

    VKBuffer* TryGetReservedBuffer(std::size_t size, bool host_visible);

    VKBuffer& CreateStagingBuffer(std::size_t size, bool host_visible);

    StagingBuffersCache& GetCache(bool host_visible);

    void ReleaseCache(bool host_visible);

    u64 ReleaseLevel(StagingBuffersCache& cache, std::size_t log2);

    const Device& device;
    VKMemoryManager& memory_manager;
    VKScheduler& scheduler;

    StagingBuffersCache host_staging_buffers;
    StagingBuffersCache device_staging_buffers;

    std::size_t current_delete_level = 0;
};

} // namespace Vulkan
