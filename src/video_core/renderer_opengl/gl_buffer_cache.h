// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <tuple>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

namespace OpenGL {

class RasterizerOpenGL;

class CachedBufferEntry final : public RasterizerCacheObject {
public:
    explicit CachedBufferEntry(VAddr cpu_addr, u8* host_ptr);

    VAddr GetCpuAddr() const override {
        return cpu_addr;
    }

    std::size_t GetSizeInBytes() const override {
        return size;
    }

    std::size_t GetSize() const {
        return size;
    }

    std::size_t GetCapacity() const {
        return capacity;
    }

    bool IsInternalized() const {
        return is_internal;
    }

    GLuint GetBuffer() const {
        return buffer.handle;
    }

    void SetSize(std::size_t new_size) {
        size = new_size;
    }

    void SetInternalState(bool is_internal_) {
        is_internal = is_internal_;
    }

    void SetCapacity(OGLBuffer&& new_buffer, std::size_t new_capacity) {
        capacity = new_capacity;
        buffer = std::move(new_buffer);
    }

private:
    VAddr cpu_addr{};
    std::size_t size{};
    std::size_t capacity{};
    bool is_internal{};
    OGLBuffer buffer;
};

class OGLBufferCache final : public RasterizerCache<std::shared_ptr<CachedBufferEntry>> {
    using BufferInfo = std::pair<GLuint, GLintptr>;

public:
    explicit OGLBufferCache(RasterizerOpenGL& rasterizer, std::size_t size);
    ~OGLBufferCache();

    void Unregister(const std::shared_ptr<CachedBufferEntry>& entry) override;

    /// Uploads data from a guest GPU address. Returns the OpenGL buffer where it's located and its
    /// offset.
    BufferInfo UploadMemory(GPUVAddr gpu_addr, std::size_t size, std::size_t alignment = 4,
                            bool internalize = false);

    /// Uploads from a host memory. Returns the OpenGL buffer where it's located and its offset.
    BufferInfo UploadHostMemory(const void* raw_pointer, std::size_t size,
                                std::size_t alignment = 4);

    bool Map(std::size_t max_size);
    void Unmap();

protected:
    // We do not have to flush this cache as things in it are never modified by us.
    void FlushObjectInner(const std::shared_ptr<CachedBufferEntry>& object) override {}

private:
    BufferInfo StreamBufferUpload(const void* raw_pointer, std::size_t size, std::size_t alignment);

    BufferInfo FixedBufferUpload(GPUVAddr gpu_addr, u8* host_ptr, std::size_t size,
                                 bool internalize);

    void GrowBuffer(std::shared_ptr<CachedBufferEntry>& entry, std::size_t new_size);

    std::shared_ptr<CachedBufferEntry> GetUncachedBuffer(VAddr cpu_addr, u8* host_ptr);

    std::shared_ptr<CachedBufferEntry> TryGetReservedBuffer(u8* host_ptr);

    void ReserveBuffer(std::shared_ptr<CachedBufferEntry> entry);

    void AlignBuffer(std::size_t alignment);

    u8* buffer_ptr = nullptr;
    GLintptr buffer_offset = 0;
    GLintptr buffer_offset_base = 0;

    OGLStreamBuffer stream_buffer;
    std::unordered_set<CacheAddr> internalized_entries;
    std::unordered_map<CacheAddr, std::vector<std::shared_ptr<CachedBufferEntry>>> buffer_reserve;
};

} // namespace OpenGL
