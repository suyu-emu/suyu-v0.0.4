// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <tuple>
#include <utility>

#include "common/common_types.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

namespace OpenGL {

class RasterizerOpenGL;

class CachedBufferEntry final : public RasterizerCacheObject {
public:
    explicit CachedBufferEntry(VAddr cpu_addr, u8* host_ptr, std::size_t size,
                               std::size_t alignment, GLuint buffer, GLintptr offset);

    VAddr GetCpuAddr() const override {
        return cpu_addr;
    }

    std::size_t GetSizeInBytes() const override {
        return size;
    }

    std::size_t GetSize() const {
        return size;
    }

    std::size_t GetAlignment() const {
        return alignment;
    }

    GLuint GetBuffer() const {
        return buffer;
    }

    GLintptr GetOffset() const {
        return offset;
    }

private:
    VAddr cpu_addr{};
    std::size_t size{};
    std::size_t alignment{};

    GLuint buffer{};
    GLintptr offset{};
};

class OGLBufferCache final : public RasterizerCache<std::shared_ptr<CachedBufferEntry>> {
public:
    explicit OGLBufferCache(RasterizerOpenGL& rasterizer, std::size_t size);

    /// Uploads data from a guest GPU address. Returns the OpenGL buffer where it's located and its
    /// offset.
    std::pair<GLuint, GLintptr> UploadMemory(GPUVAddr gpu_addr, std::size_t size,
                                             std::size_t alignment = 4, bool cache = true);

    /// Uploads from a host memory. Returns the OpenGL buffer where it's located and its offset.
    std::pair<GLuint, GLintptr> UploadHostMemory(const void* raw_pointer, std::size_t size,
                                                 std::size_t alignment = 4);

    bool Map(std::size_t max_size);
    void Unmap();

protected:
    void AlignBuffer(std::size_t alignment);

    // We do not have to flush this cache as things in it are never modified by us.
    void FlushObjectInner(const std::shared_ptr<CachedBufferEntry>& object) override {}

private:
    OGLStreamBuffer stream_buffer;

    u8* buffer_ptr = nullptr;
    GLintptr buffer_offset = 0;
    GLintptr buffer_offset_base = 0;
};

} // namespace OpenGL
