// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <optional>
#include <tuple>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"

namespace Vulkan {

class VKDevice;
class VKFence;
class VKFenceWatch;
class VKResourceManager;
class VKScheduler;

class VKStreamBuffer {
public:
    explicit VKStreamBuffer(const VKDevice& device, VKMemoryManager& memory_manager,
                            VKScheduler& scheduler, u64 size, vk::BufferUsageFlags usage,
                            vk::AccessFlags access, vk::PipelineStageFlags pipeline_stage);
    ~VKStreamBuffer();

    /**
     * Reserves a region of memory from the stream buffer.
     * @param size Size to reserve.
     * @returns A tuple in the following order: Raw memory pointer (with offset added), buffer
     * offset and a boolean that's true when buffer has been invalidated.
     */
    std::tuple<u8*, u64, bool> Reserve(u64 size);

    /// Ensures that "size" bytes of memory are available to the GPU, potentially recording a copy.
    [[nodiscard]] VKExecutionContext Send(VKExecutionContext exctx, u64 size);

    vk::Buffer GetBuffer() const {
        return *buffer;
    }

private:
    /// Creates Vulkan buffer handles committing the required the required memory.
    void CreateBuffers(VKMemoryManager& memory_manager, vk::BufferUsageFlags usage);

    /// Increases the amount of watches available.
    void ReserveWatches(std::size_t grow_size);

    const VKDevice& device;                      ///< Vulkan device manager.
    VKScheduler& scheduler;                      ///< Command scheduler.
    const u64 buffer_size;                       ///< Total size of the stream buffer.
    const vk::AccessFlags access;                ///< Access usage of this stream buffer.
    const vk::PipelineStageFlags pipeline_stage; ///< Pipeline usage of this stream buffer.

    UniqueBuffer buffer;   ///< Mapped buffer.
    VKMemoryCommit commit; ///< Memory commit.
    u8* mapped_pointer{};  ///< Pointer to the host visible commit

    u64 offset{};      ///< Buffer iterator.
    u64 mapped_size{}; ///< Size reserved for the current copy.

    std::vector<std::unique_ptr<VKFenceWatch>> watches; ///< Total watches
    std::size_t used_watches{}; ///< Count of watches, reset on invalidation.
    std::optional<std::size_t>
        invalidation_mark{}; ///< Number of watches used in the current invalidation.
};

} // namespace Vulkan
