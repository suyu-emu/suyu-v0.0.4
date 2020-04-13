// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>
#include <tuple>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

namespace {

constexpr u64 WATCHES_INITIAL_RESERVE = 0x4000;
constexpr u64 WATCHES_RESERVE_CHUNK = 0x1000;

constexpr u64 STREAM_BUFFER_SIZE = 256 * 1024 * 1024;

std::optional<u32> FindMemoryType(const VKDevice& device, u32 filter,
                                  VkMemoryPropertyFlags wanted) {
    const auto properties = device.GetPhysical().GetMemoryProperties();
    for (u32 i = 0; i < properties.memoryTypeCount; i++) {
        if (!(filter & (1 << i))) {
            continue;
        }
        if ((properties.memoryTypes[i].propertyFlags & wanted) == wanted) {
            return i;
        }
    }
    return std::nullopt;
}

} // Anonymous namespace

VKStreamBuffer::VKStreamBuffer(const VKDevice& device, VKScheduler& scheduler,
                               VkBufferUsageFlags usage)
    : device{device}, scheduler{scheduler} {
    CreateBuffers(usage);
    ReserveWatches(current_watches, WATCHES_INITIAL_RESERVE);
    ReserveWatches(previous_watches, WATCHES_INITIAL_RESERVE);
}

VKStreamBuffer::~VKStreamBuffer() = default;

std::tuple<u8*, u64, bool> VKStreamBuffer::Map(u64 size, u64 alignment) {
    ASSERT(size <= STREAM_BUFFER_SIZE);
    mapped_size = size;

    if (alignment > 0) {
        offset = Common::AlignUp(offset, alignment);
    }

    WaitPendingOperations(offset);

    bool invalidated = false;
    if (offset + size > STREAM_BUFFER_SIZE) {
        // The buffer would overflow, save the amount of used watches and reset the state.
        invalidation_mark = current_watch_cursor;
        current_watch_cursor = 0;
        offset = 0;

        // Swap watches and reset waiting cursors.
        std::swap(previous_watches, current_watches);
        wait_cursor = 0;
        wait_bound = 0;

        // Ensure that we don't wait for uncommitted fences.
        scheduler.Flush();

        invalidated = true;
    }

    return {memory.Map(offset, size), offset, invalidated};
}

void VKStreamBuffer::Unmap(u64 size) {
    ASSERT_MSG(size <= mapped_size, "Reserved size is too small");

    memory.Unmap();

    offset += size;

    if (current_watch_cursor + 1 >= current_watches.size()) {
        // Ensure that there are enough watches.
        ReserveWatches(current_watches, WATCHES_RESERVE_CHUNK);
    }
    auto& watch = current_watches[current_watch_cursor++];
    watch.upper_bound = offset;
    watch.fence.Watch(scheduler.GetFence());
}

void VKStreamBuffer::CreateBuffers(VkBufferUsageFlags usage) {
    VkBufferCreateInfo buffer_ci;
    buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_ci.pNext = nullptr;
    buffer_ci.flags = 0;
    buffer_ci.size = STREAM_BUFFER_SIZE;
    buffer_ci.usage = usage;
    buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    buffer_ci.queueFamilyIndexCount = 0;
    buffer_ci.pQueueFamilyIndices = nullptr;

    const auto& dev = device.GetLogical();
    buffer = dev.CreateBuffer(buffer_ci);

    const auto& dld = device.GetDispatchLoader();
    const auto requirements = dev.GetBufferMemoryRequirements(*buffer);
    // Prefer device local host visible allocations (this should hit AMD's pinned memory).
    auto type =
        FindMemoryType(device, requirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (!type) {
        // Otherwise search for a host visible allocation.
        type = FindMemoryType(device, requirements.memoryTypeBits,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        ASSERT_MSG(type, "No host visible and coherent memory type found");
    }
    VkMemoryAllocateInfo memory_ai;
    memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memory_ai.pNext = nullptr;
    memory_ai.allocationSize = requirements.size;
    memory_ai.memoryTypeIndex = *type;

    memory = dev.AllocateMemory(memory_ai);
    buffer.BindMemory(*memory, 0);
}

void VKStreamBuffer::ReserveWatches(std::vector<Watch>& watches, std::size_t grow_size) {
    watches.resize(watches.size() + grow_size);
}

void VKStreamBuffer::WaitPendingOperations(u64 requested_upper_bound) {
    if (!invalidation_mark) {
        return;
    }
    while (requested_upper_bound < wait_bound && wait_cursor < *invalidation_mark) {
        auto& watch = previous_watches[wait_cursor];
        wait_bound = watch.upper_bound;
        watch.fence.Wait();
        ++wait_cursor;
    }
}

} // namespace Vulkan
