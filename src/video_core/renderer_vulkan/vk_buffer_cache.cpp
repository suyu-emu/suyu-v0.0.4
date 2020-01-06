// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <optional>
#include <tuple>

#include "common/assert.h"
#include "common/bit_util.h"
#include "core/core.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_buffer_cache.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"

namespace Vulkan {

namespace {

const auto BufferUsage =
    vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer |
    vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer;

const auto UploadPipelineStage =
    vk::PipelineStageFlagBits::eTransfer | vk::PipelineStageFlagBits::eVertexInput |
    vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader |
    vk::PipelineStageFlagBits::eComputeShader;

const auto UploadAccessBarriers =
    vk::AccessFlagBits::eTransferRead | vk::AccessFlagBits::eShaderRead |
    vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eVertexAttributeRead |
    vk::AccessFlagBits::eIndexRead;

auto CreateStreamBuffer(const VKDevice& device, VKScheduler& scheduler) {
    return std::make_unique<VKStreamBuffer>(device, scheduler, BufferUsage);
}

} // Anonymous namespace

CachedBufferBlock::CachedBufferBlock(const VKDevice& device, VKMemoryManager& memory_manager,
                                     CacheAddr cache_addr, std::size_t size)
    : VideoCommon::BufferBlock{cache_addr, size} {
    const vk::BufferCreateInfo buffer_ci({}, static_cast<vk::DeviceSize>(size),
                                         BufferUsage | vk::BufferUsageFlagBits::eTransferSrc |
                                             vk::BufferUsageFlagBits::eTransferDst,
                                         vk::SharingMode::eExclusive, 0, nullptr);

    const auto& dld{device.GetDispatchLoader()};
    const auto dev{device.GetLogical()};
    buffer.handle = dev.createBufferUnique(buffer_ci, nullptr, dld);
    buffer.commit = memory_manager.Commit(*buffer.handle, false);
}

CachedBufferBlock::~CachedBufferBlock() = default;

VKBufferCache::VKBufferCache(VideoCore::RasterizerInterface& rasterizer, Core::System& system,
                             const VKDevice& device, VKMemoryManager& memory_manager,
                             VKScheduler& scheduler, VKStagingBufferPool& staging_pool)
    : VideoCommon::BufferCache<Buffer, vk::Buffer, VKStreamBuffer>{rasterizer, system,
                                                                   CreateStreamBuffer(device,
                                                                                      scheduler)},
      device{device}, memory_manager{memory_manager}, scheduler{scheduler}, staging_pool{
                                                                                staging_pool} {}

VKBufferCache::~VKBufferCache() = default;

Buffer VKBufferCache::CreateBlock(CacheAddr cache_addr, std::size_t size) {
    return std::make_shared<CachedBufferBlock>(device, memory_manager, cache_addr, size);
}

const vk::Buffer* VKBufferCache::ToHandle(const Buffer& buffer) {
    return buffer->GetHandle();
}

const vk::Buffer* VKBufferCache::GetEmptyBuffer(std::size_t size) {
    size = std::max(size, std::size_t(4));
    const auto& empty = staging_pool.GetUnusedBuffer(size, false);
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([size, buffer = *empty.handle](vk::CommandBuffer cmdbuf, auto& dld) {
        cmdbuf.fillBuffer(buffer, 0, size, 0, dld);
    });
    return &*empty.handle;
}

void VKBufferCache::UploadBlockData(const Buffer& buffer, std::size_t offset, std::size_t size,
                                    const u8* data) {
    const auto& staging = staging_pool.GetUnusedBuffer(size, true);
    std::memcpy(staging.commit->Map(size), data, size);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([staging = *staging.handle, buffer = *buffer->GetHandle(), offset,
                      size](auto cmdbuf, auto& dld) {
        cmdbuf.copyBuffer(staging, buffer, {{0, offset, size}}, dld);
        cmdbuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, UploadPipelineStage, {}, {},
            {vk::BufferMemoryBarrier(vk::AccessFlagBits::eTransferWrite, UploadAccessBarriers,
                                     VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, buffer,
                                     offset, size)},
            {}, dld);
    });
}

void VKBufferCache::DownloadBlockData(const Buffer& buffer, std::size_t offset, std::size_t size,
                                      u8* data) {
    const auto& staging = staging_pool.GetUnusedBuffer(size, true);
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([staging = *staging.handle, buffer = *buffer->GetHandle(), offset,
                      size](auto cmdbuf, auto& dld) {
        cmdbuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eVertexShader | vk::PipelineStageFlagBits::eFragmentShader |
                vk::PipelineStageFlagBits::eComputeShader,
            vk::PipelineStageFlagBits::eTransfer, {}, {},
            {vk::BufferMemoryBarrier(vk::AccessFlagBits::eShaderWrite,
                                     vk::AccessFlagBits::eTransferRead, VK_QUEUE_FAMILY_IGNORED,
                                     VK_QUEUE_FAMILY_IGNORED, buffer, offset, size)},
            {}, dld);
        cmdbuf.copyBuffer(buffer, staging, {{offset, 0, size}}, dld);
    });
    scheduler.Finish();

    std::memcpy(data, staging.commit->Map(size), size);
}

void VKBufferCache::CopyBlock(const Buffer& src, const Buffer& dst, std::size_t src_offset,
                              std::size_t dst_offset, std::size_t size) {
    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([src_buffer = *src->GetHandle(), dst_buffer = *dst->GetHandle(), src_offset,
                      dst_offset, size](auto cmdbuf, auto& dld) {
        cmdbuf.copyBuffer(src_buffer, dst_buffer, {{src_offset, dst_offset, size}}, dld);
        cmdbuf.pipelineBarrier(
            vk::PipelineStageFlagBits::eTransfer, UploadPipelineStage, {}, {},
            {vk::BufferMemoryBarrier(vk::AccessFlagBits::eTransferRead,
                                     vk::AccessFlagBits::eShaderWrite, VK_QUEUE_FAMILY_IGNORED,
                                     VK_QUEUE_FAMILY_IGNORED, src_buffer, src_offset, size),
             vk::BufferMemoryBarrier(vk::AccessFlagBits::eTransferWrite, UploadAccessBarriers,
                                     VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, dst_buffer,
                                     dst_offset, size)},
            {}, dld);
    });
}

} // namespace Vulkan
