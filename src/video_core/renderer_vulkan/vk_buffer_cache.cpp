// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>

#include "core/core.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

namespace {

constexpr VkBufferUsageFlags BUFFER_USAGE =
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

constexpr VkPipelineStageFlags UPLOAD_PIPELINE_STAGE =
    VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_VERTEX_INPUT_BIT |
    VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

constexpr VkAccessFlags UPLOAD_ACCESS_BARRIERS =
    VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT |
    VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT;

std::unique_ptr<VKStreamBuffer> CreateStreamBuffer(const VKDevice& device, VKScheduler& scheduler) {
    return std::make_unique<VKStreamBuffer>(device, scheduler, BUFFER_USAGE);
}

} // Anonymous namespace

Buffer::Buffer(const VKDevice& device, VKMemoryManager& memory_manager, VKScheduler& scheduler_,
               VKStagingBufferPool& staging_pool_, VAddr cpu_addr, std::size_t size)
    : VideoCommon::BufferBlock{cpu_addr, size}, scheduler{scheduler_}, staging_pool{staging_pool_} {
    VkBufferCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.size = static_cast<VkDeviceSize>(size);
    ci.usage = BUFFER_USAGE | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.queueFamilyIndexCount = 0;
    ci.pQueueFamilyIndices = nullptr;

    buffer.handle = device.GetLogical().CreateBuffer(ci);
    buffer.commit = memory_manager.Commit(buffer.handle, false);
}

Buffer::~Buffer() = default;

void Buffer::Upload(std::size_t offset, std::size_t size, const u8* data) const {
    const auto& staging = staging_pool.GetUnusedBuffer(size, true);
    std::memcpy(staging.commit->Map(size), data, size);

    scheduler.RequestOutsideRenderPassOperationContext();

    const VkBuffer handle = Handle();
    scheduler.Record([staging = *staging.handle, handle, offset, size](vk::CommandBuffer cmdbuf) {
        cmdbuf.CopyBuffer(staging, handle, VkBufferCopy{0, offset, size});

        VkBufferMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = UPLOAD_ACCESS_BARRIERS;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = handle;
        barrier.offset = offset;
        barrier.size = size;
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, UPLOAD_PIPELINE_STAGE, 0, {},
                               barrier, {});
    });
}

void Buffer::Download(std::size_t offset, std::size_t size, u8* data) const {
    const auto& staging = staging_pool.GetUnusedBuffer(size, true);
    scheduler.RequestOutsideRenderPassOperationContext();

    const VkBuffer handle = Handle();
    scheduler.Record([staging = *staging.handle, handle, offset, size](vk::CommandBuffer cmdbuf) {
        VkBufferMemoryBarrier barrier;
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.pNext = nullptr;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.buffer = handle;
        barrier.offset = offset;
        barrier.size = size;

        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_VERTEX_SHADER_BIT |
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_TRANSFER_BIT, 0, {}, barrier, {});
        cmdbuf.CopyBuffer(handle, staging, VkBufferCopy{offset, 0, size});
    });
    scheduler.Finish();

    std::memcpy(data, staging.commit->Map(size), size);
}

void Buffer::CopyFrom(const Buffer& src, std::size_t src_offset, std::size_t dst_offset,
                      std::size_t size) const {
    scheduler.RequestOutsideRenderPassOperationContext();

    const VkBuffer dst_buffer = Handle();
    scheduler.Record([src_buffer = src.Handle(), dst_buffer, src_offset, dst_offset,
                      size](vk::CommandBuffer cmdbuf) {
        cmdbuf.CopyBuffer(src_buffer, dst_buffer, VkBufferCopy{src_offset, dst_offset, size});

        std::array<VkBufferMemoryBarrier, 2> barriers;
        barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[0].pNext = nullptr;
        barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barriers[0].dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[0].buffer = src_buffer;
        barriers[0].offset = src_offset;
        barriers[0].size = size;
        barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barriers[1].pNext = nullptr;
        barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barriers[1].dstAccessMask = UPLOAD_ACCESS_BARRIERS;
        barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barriers[1].buffer = dst_buffer;
        barriers[1].offset = dst_offset;
        barriers[1].size = size;
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, UPLOAD_PIPELINE_STAGE, 0, {},
                               barriers, {});
    });
}

VKBufferCache::VKBufferCache(VideoCore::RasterizerInterface& rasterizer, Core::System& system,
                             const VKDevice& device, VKMemoryManager& memory_manager,
                             VKScheduler& scheduler, VKStagingBufferPool& staging_pool)
    : VideoCommon::BufferCache<Buffer, VkBuffer, VKStreamBuffer>{rasterizer, system,
                                                                 CreateStreamBuffer(device,
                                                                                    scheduler)},
      device{device}, memory_manager{memory_manager}, scheduler{scheduler}, staging_pool{
                                                                                staging_pool} {}

VKBufferCache::~VKBufferCache() = default;

std::shared_ptr<Buffer> VKBufferCache::CreateBlock(VAddr cpu_addr, std::size_t size) {
    return std::make_shared<Buffer>(device, memory_manager, scheduler, staging_pool, cpu_addr,
                                    size);
}

VKBufferCache::BufferInfo VKBufferCache::GetEmptyBuffer(std::size_t size) {
    size = std::max(size, std::size_t(4));
    const auto& empty = staging_pool.GetUnusedBuffer(size, false);
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([size, buffer = *empty.handle](vk::CommandBuffer cmdbuf) {
        cmdbuf.FillBuffer(buffer, 0, size, 0);
    });
    return {*empty.handle, 0, 0};
}

} // namespace Vulkan
