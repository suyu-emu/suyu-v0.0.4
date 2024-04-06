// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <climits>
#include <span>
#include <vector>

#include <Metal/Metal.hpp>

#include "common/common_types.h"

namespace Metal {

class Device;
class CommandRecorder;

enum class MemoryUsage {
    DeviceLocal,
    Upload,
    Download,
};

struct StagingBufferRef {
    StagingBufferRef(MTL::Buffer* buffer_, size_t offset_, std::span<u8> mapped_span_);
    ~StagingBufferRef();

    MTL::Buffer* buffer;
    size_t offset;
    std::span<u8> mapped_span;
};

struct StagingBuffer {
    StagingBuffer(MTL::Buffer* buffer_, std::span<u8> mapped_span_);
    ~StagingBuffer();

    MTL::Buffer* buffer;
    std::span<u8> mapped_span;

    StagingBufferRef Ref() const noexcept;
};

class StagingBufferPool {
public:
    static constexpr size_t NUM_SYNCS = 16;

    explicit StagingBufferPool(const Device& device, CommandRecorder& command_recorder_);
    ~StagingBufferPool();

    StagingBufferRef Request(size_t size, MemoryUsage usage, bool deferred = false);
    void FreeDeferred(StagingBufferRef& ref);

    [[nodiscard]] MTL::Buffer* GetSTreamBufferHandle() const noexcept {
        return stream_buffer;
    }

    void TickFrame();

private:
    struct StagingBuffers {
        std::vector<StagingBuffer> entries;
        size_t delete_index = 0;
        size_t iterate_index = 0;
    };

    static constexpr size_t NUM_LEVELS = sizeof(size_t) * CHAR_BIT;
    using StagingBuffersCache = std::array<StagingBuffers, NUM_LEVELS>;

    StagingBufferRef GetStreamBuffer(size_t size);

    StagingBufferRef GetStagingBuffer(size_t size, MemoryUsage usage, bool deferred = false);

    StagingBufferRef CreateStagingBuffer(size_t size, MemoryUsage usage, bool deferred);

    StagingBuffersCache& GetCache(MemoryUsage usage);

    void ReleaseCache(MemoryUsage usage);

    void ReleaseLevel(StagingBuffersCache& cache, size_t log2);

    const Device& device;
    CommandRecorder& command_recorder;

    MTL::Buffer* stream_buffer{};

    size_t iterator = 0;
    size_t used_iterator = 0;
    size_t free_iterator = 0;
    std::array<u64, NUM_SYNCS> sync_ticks{};

    StagingBuffersCache device_local_cache;
    StagingBuffersCache upload_cache;
    StagingBuffersCache download_cache;

    size_t current_delete_level = 0;
    u64 buffer_index = 0;
    u64 unique_ids{};
};

} // namespace Metal
