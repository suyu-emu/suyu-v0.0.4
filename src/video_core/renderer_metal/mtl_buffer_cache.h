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

struct BufferView {
    BufferView(MTL::Buffer* buffer_, size_t offset_, size_t size_,
               VideoCore::Surface::PixelFormat format_ = VideoCore::Surface::PixelFormat::Invalid);
    ~BufferView();

    MTL::Buffer* buffer = nullptr;
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
        // TODO: track usage?
    }

    [[nodiscard]] MTL::Buffer* Handle() const noexcept {
        return buffer;
    }

    operator MTL::Buffer*() const noexcept {
        return buffer;
    }

private:
    MTL::Buffer* buffer = nil;
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

    void CopyBuffer(MTL::Buffer* src_buffer, MTL::Buffer* dst_buffer,
                    std::span<const VideoCommon::BufferCopy> copies, bool barrier,
                    bool can_reorder_upload = false);

    void PostCopyBarrier() {}

    void ClearBuffer(MTL::Buffer* dest_buffer, u32 offset, size_t size, u32 value);

    void BindIndexBuffer(PrimitiveTopology topology, IndexFormat index_format, u32 num_indices,
                         u32 base_vertex, MTL::Buffer* buffer, u32 offset, u32 size);

    void BindQuadIndexBuffer(PrimitiveTopology topology, u32 first, u32 count);

    void BindVertexBuffer(u32 index, MTL::Buffer* buffer, u32 offset, u32 size, u32 stride);

    void BindVertexBuffers(VideoCommon::HostBindings<Buffer>& bindings);

    // TODO: implement
    void BindTransformFeedbackBuffer(u32 index, MTL::Buffer* buffer, u32 offset, u32 size) {}

    // TODO: implement
    void BindTransformFeedbackBuffers(VideoCommon::HostBindings<Buffer>& bindings) {}

    std::span<u8> BindMappedUniformBuffer(size_t stage, u32 binding_index, u32 size) {
        // TODO: just set bytes in case the size is <= 4KB?
        const StagingBufferRef ref = staging_pool.Request(size, MemoryUsage::Upload);
        BindBuffer(stage, binding_index, ref.buffer, static_cast<u32>(ref.offset), size);

        return ref.mapped_span;
    }

    void BindUniformBuffer(size_t stage, u32 binding_index, MTL::Buffer* buffer, u32 offset,
                           u32 size) {
        BindBuffer(stage, binding_index, buffer, offset, size);
    }

    // TODO: implement
    void BindComputeUniformBuffer(u32 binding_index, MTL::Buffer* buffer, u32 offset, u32 size) {}

    void BindStorageBuffer(size_t stage, u32 binding_index, MTL::Buffer* buffer, u32 offset,
                           u32 size, [[maybe_unused]] bool is_written) {
        BindBuffer(stage, binding_index + 8, buffer, offset, size); // HACK: offset by 8
    }

    // TODO: implement
    void BindComputeStorageBuffer(u32 binding_index, Buffer& buffer, u32 offset, u32 size,
                                  bool is_written) {}

    // TODO: implement
    void BindTextureBuffer(Buffer& buffer, u32 offset, u32 size,
                           VideoCore::Surface::PixelFormat format) {}

private:
    void BindBuffer(size_t stage, u32 binding_index, MTL::Buffer* buffer, u32 offset, u32 size);

    void ReserveNullBuffer();
    MTL::Buffer* CreateNullBuffer();

    const Device& device;
    CommandRecorder& command_recorder;
    StagingBufferPool& staging_pool;

    // Common buffers
    MTL::Buffer* null_buffer = nullptr;
    MTL::Buffer* quad_index_buffer = nullptr;
};

struct BufferCacheParams {
    using Runtime = Metal::BufferCacheRuntime;
    using Buffer = Metal::Buffer;
    using Async_Buffer = Metal::StagingBufferRef;
    using MemoryTracker = VideoCommon::MemoryTrackerBase<Tegra::MaxwellDeviceMemoryManager>;

    static constexpr bool IS_OPENGL = false;
    static constexpr bool HAS_PERSISTENT_UNIFORM_BUFFER_BINDINGS = false;
    static constexpr bool HAS_FULL_INDEX_AND_PRIMITIVE_SUPPORT = false;
    static constexpr bool NEEDS_BIND_UNIFORM_INDEX = true;
    static constexpr bool NEEDS_BIND_STORAGE_INDEX = true;
    static constexpr bool USE_MEMORY_MAPS = true;
    static constexpr bool SEPARATE_IMAGE_BUFFER_BINDINGS = false;
    static constexpr bool USE_MEMORY_MAPS_FOR_UPLOADS = true;
};

using BufferCache = VideoCommon::BufferCache<BufferCacheParams>;

} // namespace Metal
