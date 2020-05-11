// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <tuple>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;
class VKFence;
class VKFenceWatch;
class VKScheduler;

class VKStreamBuffer final {
public:
    explicit VKStreamBuffer(const VKDevice& device, VKScheduler& scheduler,
                            VkBufferUsageFlags usage);
    ~VKStreamBuffer();

    /**
     * Reserves a region of memory from the stream buffer.
     * @param size Size to reserve.
     * @returns A tuple in the following order: Raw memory pointer (with offset added), buffer
     * offset and a boolean that's true when buffer has been invalidated.
     */
    std::tuple<u8*, u64, bool> Map(u64 size, u64 alignment);

    /// Ensures that "size" bytes of memory are available to the GPU, potentially recording a copy.
    void Unmap(u64 size);

    VkBuffer Handle() const noexcept {
        return *buffer;
    }

    u64 Address() const noexcept {
        return 0;
    }

private:
    struct Watch final {
        VKFenceWatch fence;
        u64 upper_bound{};
    };

    /// Creates Vulkan buffer handles committing the required the required memory.
    void CreateBuffers(VkBufferUsageFlags usage);

    /// Increases the amount of watches available.
    void ReserveWatches(std::vector<Watch>& watches, std::size_t grow_size);

    void WaitPendingOperations(u64 requested_upper_bound);

    const VKDevice& device; ///< Vulkan device manager.
    VKScheduler& scheduler; ///< Command scheduler.

    vk::Buffer buffer;        ///< Mapped buffer.
    vk::DeviceMemory memory;  ///< Memory allocation.
    u64 stream_buffer_size{}; ///< Stream buffer size.

    u64 offset{};      ///< Buffer iterator.
    u64 mapped_size{}; ///< Size reserved for the current copy.

    std::vector<Watch> current_watches;           ///< Watches recorded in the current iteration.
    std::size_t current_watch_cursor{};           ///< Count of watches, reset on invalidation.
    std::optional<std::size_t> invalidation_mark; ///< Number of watches used in the previous cycle.

    std::vector<Watch> previous_watches; ///< Watches used in the previous iteration.
    std::size_t wait_cursor{};           ///< Last watch being waited for completion.
    u64 wait_bound{};                    ///< Highest offset being watched for completion.
};

} // namespace Vulkan
