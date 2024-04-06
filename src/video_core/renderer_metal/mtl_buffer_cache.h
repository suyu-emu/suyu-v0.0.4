// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/buffer_cache/buffer_cache_base.h"
#include "video_core/buffer_cache/memory_tracker_base.h"
#include "video_core/buffer_cache/usage_tracker.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_metal/mtl_staging_buffer_pool.h"
#include "video_core/surface.h"

namespace Metal {

class Device;
class CommandRecorder;

class BufferCacheRuntime;

struct BoundBuffer {
    BoundBuffer() = default;
    BoundBuffer(MTLBuffer_t buffer_, size_t offset_, size_t size_);

    ~BoundBuffer();

    MTLBuffer_t buffer = nil;
    size_t offset{};
    size_t size{};
};

struct BufferView {
    BufferView(MTLBuffer_t buffer_, size_t offset_, size_t size_,
               VideoCore::Surface::PixelFormat format_ = VideoCore::Surface::PixelFormat::Invalid);
    ~BufferView();

    MTLBuffer_t buffer = nil;
    size_t offset{};
    size_t size{};
    VideoCore::Surface::PixelFormat format{};
};

class Buffer : public VideoCommon::BufferBase {
public:
    explicit Buffer(BufferCacheRuntime&, VideoCommon::NullBufferParams null_params);
    explicit Buffer(BufferCacheRuntime& runtime, VAddr cpu_addr_, u64 size_bytes_);

    [[nodiscard]] BufferView View(u32 offset, u32 size, VideoCore::Surface::PixelFormat format);

    void MarkUsage(u64 offset, u64 size) noexcept {
        // TODO: track usage
    }

    [[nodiscard]] MTLBuffer_t Handle() const noexcept {
        return buffer;
    }

    operator MTLBuffer_t() const noexcept {
        return buffer;
    }

private:
    MTLBuffer_t buffer = nil;
    bool is_null{};

    BufferView view;
};

class BufferCacheRuntime {
    friend Buffer;

    using PrimitiveTopology = Tegra::Engines::Maxwell3D::Regs::PrimitiveTopology;
    using IndexFormat = Tegra::Engines::Maxwell3D::Regs::IndexFormat;

public:
    static constexpr size_t NULL_BUFFER_SIZE = 4;
    static constexpr size_t MAX_METAL_BUFFERS = 31;

    explicit BufferCacheRuntime(const Device& device_, CommandRecorder& command_recorder_,
                                StagingBufferPool& staging_pool_);

    void TickFrame(Common::SlotVector<Buffer>& slot_buffers) noexcept;

    void Finish();

    u64 GetDeviceLocalMemory() const {
        return 0;
    }

    u64 GetDeviceMemoryUsage() const {
        return 0;
    }

    bool CanReportMemoryUsage() const {
        return false;
    }

    u32 GetStorageBufferAlignment() const;

    [[nodiscard]] StagingBufferRef UploadStagingBuffer(size_t size);

    [[nodiscard]] StagingBufferRef DownloadStagingBuffer(size_t size, bool deferred = false);

    bool CanReorderUpload(const Buffer& buffer, std::span<const VideoCommon::BufferCopy> copies) {
        return false;
    }

    void FreeDeferredStagingBuffer(StagingBufferRef& ref);

    void PreCopyBarrier() {}

    void CopyBuffer(MTLBuffer_t src_buffer, MTLBuffer_t dst_buffer,
                    std::span<const VideoCommon::BufferCopy> copies, bool barrier,
                    bool can_reorder_upload = false);

    void PostCopyBarrier() {}

    void ClearBuffer(MTLBuffer_t dest_buffer, u32 offset, size_t size, u32 value);

    void BindIndexBuffer(PrimitiveTopology topology, IndexFormat index_format, u32 num_indices,
                         u32 base_vertex, MTLBuffer_t buffer, u32 offset, u32 size);

    void BindQuadIndexBuffer(PrimitiveTopology topology, u32 first, u32 count);

    void BindVertexBuffer(u32 index, MTLBuffer_t buffer, u32 offset, u32 size, u32 stride);

    void BindVertexBuffers(VideoCommon::HostBindings<Buffer>& bindings);

    // TODO: implement
    void BindTransformFeedbackBuffer(u32 index, MTLBuffer_t buffer, u32 offset, u32 size) {}

    // TODO: implement
    void BindTransformFeedbackBuffers(VideoCommon::HostBindings<Buffer>& bindings) {}

    std::span<u8> BindMappedUniformBuffer([[maybe_unused]] size_t stage,
                                          [[maybe_unused]] u32 binding_index, u32 size) {
        const StagingBufferRef ref = staging_pool.Request(size, MemoryUsage::Upload);
        BindBuffer(ref.buffer, static_cast<u32>(ref.offset), size);
        return ref.mapped_span;
    }

    void BindUniformBuffer(MTLBuffer_t buffer, u32 offset, u32 size) {
        BindBuffer(buffer, offset, size);
    }

    void BindStorageBuffer(MTLBuffer_t buffer, u32 offset, u32 size,
                           [[maybe_unused]] bool is_written) {
        BindBuffer(buffer, offset, size);
    }

    // TODO: implement
    void BindTextureBuffer(Buffer& buffer, u32 offset, u32 size,
                           VideoCore::Surface::PixelFormat format) {}

private:
    void BindBuffer(MTLBuffer_t buffer, u32 offset, u32 size) {
        // FIXME: what should be the index?
        bound_buffers[0] = BoundBuffer(buffer, offset, size);
    }

    void ReserveNullBuffer();
    MTLBuffer_t CreateNullBuffer();

    const Device& device;
    CommandRecorder& command_recorder;
    StagingBufferPool& staging_pool;

    // Common buffers
    MTLBuffer_t null_buffer = nil;
    MTLBuffer_t quad_index_buffer = nil;

    // TODO: probably move this into a separate class
    // Bound state
    // Vertex buffers are bound to MAX_METAL_BUFFERS - index - 1, while regular buffers are bound to
    // index
    BoundBuffer bound_vertex_buffers[MAX_METAL_BUFFERS] = {{}};
    struct {
        BoundBuffer buffer;
        // TODO: include index type and primitive topology
    } bound_index_buffer = {};
    BoundBuffer bound_buffers[MAX_METAL_BUFFERS] = {{}};
};

struct BufferCacheParams {
    using Runtime = Metal::BufferCacheRuntime;
    using Buffer = Metal::Buffer;
    using Async_Buffer = Metal::StagingBufferRef;
    using MemoryTracker = VideoCommon::MemoryTrackerBase<Tegra::MaxwellDeviceMemoryManager>;

    static constexpr bool IS_OPENGL = false;
    static constexpr bool HAS_PERSISTENT_UNIFORM_BUFFER_BINDINGS = false;
    static constexpr bool HAS_FULL_INDEX_AND_PRIMITIVE_SUPPORT = false;
    static constexpr bool NEEDS_BIND_UNIFORM_INDEX = false;
    static constexpr bool NEEDS_BIND_STORAGE_INDEX = false;
    static constexpr bool USE_MEMORY_MAPS = true;
    static constexpr bool SEPARATE_IMAGE_BUFFER_BINDINGS = false;
    static constexpr bool USE_MEMORY_MAPS_FOR_UPLOADS = true;
};

using BufferCache = VideoCommon::BufferCache<BufferCacheParams>;

} // namespace Metal
