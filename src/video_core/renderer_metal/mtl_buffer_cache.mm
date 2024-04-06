// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <cstring>
#include <span>
#include <vector>

#include "video_core/renderer_metal/mtl_buffer_cache.h"

#include "video_core/renderer_metal/mtl_device.h"

namespace Metal {

namespace {

MTLBuffer_t CreatePrivateBuffer(const Device& device, size_t size) {
    return [device.GetDevice() newBufferWithLength:size options:MTLResourceStorageModePrivate];
}

} // Anonymous namespace

BoundBuffer::BoundBuffer(MTLBuffer_t buffer_, size_t offset_, size_t size_)
    : buffer{[buffer_ retain]}, offset{offset_}, size{size_} {}

BoundBuffer::~BoundBuffer() {
    if (buffer) {
        [buffer release];
    }
}

BufferView::BufferView(MTLBuffer_t buffer_, size_t offset_, size_t size_,
                       VideoCore::Surface::PixelFormat format_)
    : buffer{[buffer_ retain]}, offset{offset_}, size{size_}, format{format_} {}

BufferView::~BufferView() {
    [buffer release];
}

Buffer::Buffer(BufferCacheRuntime& runtime, VideoCommon::NullBufferParams null_params)
    : VideoCommon::BufferBase(null_params), buffer{runtime.CreateNullBuffer()},
      is_null{true}, view(buffer, 0, BufferCacheRuntime::NULL_BUFFER_SIZE) {}

Buffer::Buffer(BufferCacheRuntime& runtime, DAddr cpu_addr_, u64 size_bytes_)
    : VideoCommon::BufferBase(cpu_addr_, size_bytes_),
      buffer{CreatePrivateBuffer(runtime.device, size_bytes_)},
      view(buffer, 0, size_bytes_) {}

BufferView Buffer::View(u32 offset, u32 size, VideoCore::Surface::PixelFormat format) {
    return BufferView(buffer, offset, size, format);
}

BufferCacheRuntime::BufferCacheRuntime(const Device& device_, CommandRecorder& command_recorder_,
                                       StagingBufferPool& staging_pool_)
    : device{device_}, command_recorder{command_recorder_}, staging_pool{staging_pool_} {
    // TODO: create quad index buffer
}

StagingBufferRef BufferCacheRuntime::UploadStagingBuffer(size_t size) {
    return staging_pool.Request(size, MemoryUsage::Upload);
}

StagingBufferRef BufferCacheRuntime::DownloadStagingBuffer(size_t size, bool deferred) {
    return staging_pool.Request(size, MemoryUsage::Download, deferred);
}

void BufferCacheRuntime::FreeDeferredStagingBuffer(StagingBufferRef& ref) {
    staging_pool.FreeDeferred(ref);
}

u32 BufferCacheRuntime::GetStorageBufferAlignment() const {
    // TODO: do not hardcode this
    return 4;
}

void BufferCacheRuntime::TickFrame(Common::SlotVector<Buffer>& slot_buffers) noexcept {}

void BufferCacheRuntime::Finish() {}

void BufferCacheRuntime::CopyBuffer(MTLBuffer_t dst_buffer, MTLBuffer_t src_buffer,
                                    std::span<const VideoCommon::BufferCopy> copies, bool barrier,
                                    bool can_reorder_upload) {
    // TODO: copy buffer
}

void BufferCacheRuntime::ClearBuffer(MTLBuffer_t dest_buffer, u32 offset, size_t size, u32 value) {
    // TODO: clear buffer
}

void BufferCacheRuntime::BindIndexBuffer(PrimitiveTopology topology, IndexFormat index_format,
                                         u32 base_vertex, u32 num_indices, MTLBuffer_t buffer,
                                         u32 offset, [[maybe_unused]] u32 size) {
    // TODO: convert parameters to Metal enums
    bound_index_buffer = {BoundBuffer(buffer, offset, size)};
}

void BufferCacheRuntime::BindQuadIndexBuffer(PrimitiveTopology topology, u32 first, u32 count) {
    // TODO: bind quad index buffer
}

void BufferCacheRuntime::BindVertexBuffer(u32 index, MTLBuffer_t buffer, u32 offset, u32 size,
                                          u32 stride) {
    // TODO: use stride
    bound_vertex_buffers[MAX_METAL_BUFFERS - index - 1] = {BoundBuffer(buffer, offset, size)};
}

void BufferCacheRuntime::BindVertexBuffers(VideoCommon::HostBindings<Buffer>& bindings) {
    // TODO: implement
}

void BufferCacheRuntime::ReserveNullBuffer() {
    if (!null_buffer) {
        null_buffer = CreateNullBuffer();
    }
}

MTLBuffer_t BufferCacheRuntime::CreateNullBuffer() {
    return [device.GetDevice() newBufferWithLength:NULL_BUFFER_SIZE
                                           options:MTLResourceStorageModePrivate];
}

} // namespace Vulkan
