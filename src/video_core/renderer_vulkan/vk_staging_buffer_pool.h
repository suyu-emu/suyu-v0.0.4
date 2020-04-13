// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <climits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/common_types.h"

#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;
class VKFenceWatch;
class VKScheduler;

struct VKBuffer final {
    vk::Buffer handle;
    VKMemoryCommit commit;
};

class VKStagingBufferPool final {
public:
    explicit VKStagingBufferPool(const VKDevice& device, VKMemoryManager& memory_manager,
                                 VKScheduler& scheduler);
    ~VKStagingBufferPool();

    VKBuffer& GetUnusedBuffer(std::size_t size, bool host_visible);

    void TickFrame();

private:
    struct StagingBuffer final {
        explicit StagingBuffer(std::unique_ptr<VKBuffer> buffer, VKFence& fence, u64 last_epoch);
        StagingBuffer(StagingBuffer&& rhs) noexcept;
        StagingBuffer(const StagingBuffer&) = delete;
        ~StagingBuffer();

        StagingBuffer& operator=(StagingBuffer&& rhs) noexcept;

        std::unique_ptr<VKBuffer> buffer;
        VKFenceWatch watch;
        u64 last_epoch = 0;
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

    const VKDevice& device;
    VKMemoryManager& memory_manager;
    VKScheduler& scheduler;
    const bool is_device_integrated;

    StagingBuffersCache host_staging_buffers;
    StagingBuffersCache device_staging_buffers;

    u64 epoch = 0;

    std::size_t current_delete_level = 0;
};

} // namespace Vulkan
