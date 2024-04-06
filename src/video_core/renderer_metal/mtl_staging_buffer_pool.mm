// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/bit_util.h"
#include "common/common_types.h"
#include "common/literals.h"
#include "video_core/renderer_metal/mtl_command_recorder.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_staging_buffer_pool.h"

namespace Metal {

StagingBufferRef::StagingBufferRef(MTLBuffer_t buffer_, size_t offset_, std::span<u8> mapped_span_)
    : buffer{[buffer_ retain]}, offset{offset_}, mapped_span{mapped_span_} {}

StagingBufferRef::~StagingBufferRef() {
    [buffer release];
}

StagingBuffer::StagingBuffer(MTLBuffer_t buffer_, std::span<u8> mapped_span_)
    : buffer{[buffer_ retain]}, mapped_span{mapped_span_} {}

StagingBuffer::~StagingBuffer() {
    [buffer release];
}

StagingBufferRef StagingBuffer::Ref() const noexcept {
    return StagingBufferRef(buffer, 0, mapped_span);
}

// TODO: use the _MiB suffix
constexpr size_t STREAM_BUFFER_SIZE = 128 * 1024 * 1024;//128_MiB;
constexpr size_t REGION_SIZE = STREAM_BUFFER_SIZE / StagingBufferPool::NUM_SYNCS;

StagingBufferPool::StagingBufferPool(const Device& device_, CommandRecorder& command_recorder_)
    : device{device_}, command_recorder{command_recorder_} {
    stream_buffer = [device.GetDevice() newBufferWithLength:STREAM_BUFFER_SIZE
                                                    options:MTLResourceStorageModePrivate];
}

StagingBufferPool::~StagingBufferPool() = default;

StagingBufferRef StagingBufferPool::Request(size_t size, MemoryUsage usage, bool deferred) {
    if (!deferred && usage == MemoryUsage::Upload && size <= REGION_SIZE) {
        return GetStreamBuffer(size);
    }

    return GetStagingBuffer(size, usage, deferred);
}

void StagingBufferPool::FreeDeferred(StagingBufferRef& ref) {
   // TODO: implement this
}

void StagingBufferPool::TickFrame() {
    current_delete_level = (current_delete_level + 1) % NUM_LEVELS;

    ReleaseCache(MemoryUsage::DeviceLocal);
    ReleaseCache(MemoryUsage::Upload);
    ReleaseCache(MemoryUsage::Download);
}

StagingBufferRef StagingBufferPool::GetStreamBuffer(size_t size) {
    // TODO: implement this

    // HACK
    return GetStagingBuffer(size, MemoryUsage::Upload);
}

StagingBufferRef StagingBufferPool::GetStagingBuffer(size_t size, MemoryUsage usage,
                                                     bool deferred) {
    return CreateStagingBuffer(size, usage, deferred);
}

StagingBufferRef StagingBufferPool::CreateStagingBuffer(size_t size, MemoryUsage usage,
                                                        bool deferred) {
    const u32 log2 = Common::Log2Ceil64(size);
    MTLBuffer_t buffer = [device.GetDevice() newBufferWithLength:size
                                                         options:MTLResourceStorageModePrivate];
    // TODO: check if the mapped span is correct
    std::span<u8> mapped_span(static_cast<u8*>([buffer contents]), size);
    auto& entry = GetCache(usage)[log2].entries.emplace_back(buffer, mapped_span);

    return entry.Ref();
}

StagingBufferPool::StagingBuffersCache& StagingBufferPool::GetCache(MemoryUsage usage) {
    switch (usage) {
    case MemoryUsage::DeviceLocal:
        return device_local_cache;
    case MemoryUsage::Upload:
        return upload_cache;
    case MemoryUsage::Download:
        return download_cache;
    default:
        ASSERT_MSG(false, "Invalid memory usage={}", usage);
        return upload_cache;
    }
}

void StagingBufferPool::ReleaseCache(MemoryUsage usage) {
    ReleaseLevel(GetCache(usage), current_delete_level);
}

void StagingBufferPool::ReleaseLevel(StagingBuffersCache& cache, size_t log2) {
    // TODO: implement this
}

} // namespace Metal
