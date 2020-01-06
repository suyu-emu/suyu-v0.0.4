// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>
#include <tuple>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"

namespace Vulkan {

namespace {

constexpr u64 WATCHES_INITIAL_RESERVE = 0x4000;
constexpr u64 WATCHES_RESERVE_CHUNK = 0x1000;

constexpr u64 STREAM_BUFFER_SIZE = 256 * 1024 * 1024;

std::optional<u32> FindMemoryType(const VKDevice& device, u32 filter,
                                  vk::MemoryPropertyFlags wanted) {
    const auto properties = device.GetPhysical().getMemoryProperties(device.GetDispatchLoader());
    for (u32 i = 0; i < properties.memoryTypeCount; i++) {
        if (!(filter & (1 << i))) {
            continue;
        }
        if ((properties.memoryTypes[i].propertyFlags & wanted) == wanted) {
            return i;
        }
    }
    return {};
}

} // Anonymous namespace

VKStreamBuffer::VKStreamBuffer(const VKDevice& device, VKScheduler& scheduler,
                               vk::BufferUsageFlags usage)
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

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    const auto pointer = reinterpret_cast<u8*>(dev.mapMemory(*memory, offset, size, {}, dld));
    return {pointer, offset, invalidated};
}

void VKStreamBuffer::Unmap(u64 size) {
    ASSERT_MSG(size <= mapped_size, "Reserved size is too small");

    const auto dev = device.GetLogical();
    dev.unmapMemory(*memory, device.GetDispatchLoader());

    offset += size;

    if (current_watch_cursor + 1 >= current_watches.size()) {
        // Ensure that there are enough watches.
        ReserveWatches(current_watches, WATCHES_RESERVE_CHUNK);
    }
    auto& watch = current_watches[current_watch_cursor++];
    watch.upper_bound = offset;
    watch.fence.Watch(scheduler.GetFence());
}

void VKStreamBuffer::CreateBuffers(vk::BufferUsageFlags usage) {
    const vk::BufferCreateInfo buffer_ci({}, STREAM_BUFFER_SIZE, usage, vk::SharingMode::eExclusive,
                                         0, nullptr);
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    buffer = dev.createBufferUnique(buffer_ci, nullptr, dld);

    const auto requirements = dev.getBufferMemoryRequirements(*buffer, dld);
    // Prefer device local host visible allocations (this should hit AMD's pinned memory).
    auto type = FindMemoryType(device, requirements.memoryTypeBits,
                               vk::MemoryPropertyFlagBits::eHostVisible |
                                   vk::MemoryPropertyFlagBits::eHostCoherent |
                                   vk::MemoryPropertyFlagBits::eDeviceLocal);
    if (!type) {
        // Otherwise search for a host visible allocation.
        type = FindMemoryType(device, requirements.memoryTypeBits,
                              vk::MemoryPropertyFlagBits::eHostVisible |
                                  vk::MemoryPropertyFlagBits::eHostCoherent);
        ASSERT_MSG(type, "No host visible and coherent memory type found");
    }
    const vk::MemoryAllocateInfo alloc_ci(requirements.size, *type);
    memory = dev.allocateMemoryUnique(alloc_ci, nullptr, dld);

    dev.bindBufferMemory(*buffer, *memory, 0, dld);
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
