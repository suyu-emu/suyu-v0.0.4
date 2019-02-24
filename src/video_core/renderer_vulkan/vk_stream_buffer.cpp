// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "common/assert.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"

namespace Vulkan {

constexpr u64 WATCHES_INITIAL_RESERVE = 0x4000;
constexpr u64 WATCHES_RESERVE_CHUNK = 0x1000;

VKStreamBuffer::VKStreamBuffer(const VKDevice& device, VKMemoryManager& memory_manager,
                               VKScheduler& scheduler, u64 size, vk::BufferUsageFlags usage,
                               vk::AccessFlags access, vk::PipelineStageFlags pipeline_stage)
    : device{device}, scheduler{scheduler},
      has_device_exclusive_memory{!memory_manager.IsMemoryUnified()},
      buffer_size{size}, access{access}, pipeline_stage{pipeline_stage} {
    CreateBuffers(memory_manager, usage);
    ReserveWatches(WATCHES_INITIAL_RESERVE);
}

VKStreamBuffer::~VKStreamBuffer() = default;

std::tuple<u8*, u64, vk::Buffer, bool> VKStreamBuffer::Reserve(u64 size, bool keep_in_host) {
    ASSERT(size <= buffer_size);
    mapped_size = size;

    if (offset + size > buffer_size) {
        // The buffer would overflow, save the amount of used buffers, signal an invalidation and
        // reset the state.
        invalidation_mark = used_watches;
        used_watches = 0;
        offset = 0;
    }

    use_device = has_device_exclusive_memory && !keep_in_host;

    const vk::Buffer buffer = use_device ? *device_buffer : *mappable_buffer;
    return {mapped_pointer + offset, offset, buffer, invalidation_mark.has_value()};
}

VKExecutionContext VKStreamBuffer::Send(VKExecutionContext exctx, u64 size) {
    ASSERT_MSG(size <= mapped_size, "Reserved size is too small");

    if (invalidation_mark) {
        // TODO(Rodrigo): Find a better way to invalidate than waiting for all watches to finish.
        exctx = scheduler.Flush();
        std::for_each(watches.begin(), watches.begin() + *invalidation_mark,
                      [&](auto& resource) { resource->Wait(); });
        invalidation_mark = std::nullopt;
    }

    // Only copy to VRAM when requested.
    if (use_device) {
        const auto& dld = device.GetDispatchLoader();
        const u32 graphics_family = device.GetGraphicsFamily();
        const auto cmdbuf = exctx.GetCommandBuffer();

        // Buffers are mirrored, that's why the copy is done with the same offset on both buffers.
        const vk::BufferCopy copy_region(offset, offset, size);
        cmdbuf.copyBuffer(*mappable_buffer, *device_buffer, {copy_region}, dld);

        // Protect the buffer from GPU usage until the copy has finished.
        const vk::BufferMemoryBarrier barrier(vk::AccessFlagBits::eTransferWrite, access,
                                              graphics_family, graphics_family, *device_buffer,
                                              offset, size);
        cmdbuf.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, pipeline_stage, {}, {},
                               {barrier}, {}, dld);
    }

    if (used_watches + 1 >= watches.size()) {
        // Ensure that there are enough watches.
        ReserveWatches(WATCHES_RESERVE_CHUNK);
    }
    // Add a watch for this allocation.
    watches[used_watches++]->Watch(exctx.GetFence());

    offset += size;

    return exctx;
}

void VKStreamBuffer::CreateBuffers(VKMemoryManager& memory_manager, vk::BufferUsageFlags usage) {
    vk::BufferUsageFlags mappable_usage = usage;
    if (has_device_exclusive_memory) {
        mappable_usage |= vk::BufferUsageFlagBits::eTransferSrc;
    }
    const vk::BufferCreateInfo buffer_ci({}, buffer_size, mappable_usage,
                                         vk::SharingMode::eExclusive, 0, nullptr);

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    mappable_buffer = dev.createBufferUnique(buffer_ci, nullptr, dld);
    mappable_commit = memory_manager.Commit(*mappable_buffer, true);
    mapped_pointer = mappable_commit->GetData();

    if (has_device_exclusive_memory) {
        const vk::BufferCreateInfo buffer_ci({}, buffer_size,
                                             usage | vk::BufferUsageFlagBits::eTransferDst,
                                             vk::SharingMode::eExclusive, 0, nullptr);
        device_buffer = dev.createBufferUnique(buffer_ci, nullptr, dld);
        device_commit = memory_manager.Commit(*device_buffer, false);
    }
}

void VKStreamBuffer::ReserveWatches(std::size_t grow_size) {
    const std::size_t previous_size = watches.size();
    watches.resize(previous_size + grow_size);
    std::generate(watches.begin() + previous_size, watches.end(),
                  []() { return std::make_unique<VKFenceWatch>(); });
}

} // namespace Vulkan
