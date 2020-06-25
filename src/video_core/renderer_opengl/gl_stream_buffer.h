// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>
#include <glad/glad.h>
#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class Device;

class OGLStreamBuffer : private NonCopyable {
public:
    explicit OGLStreamBuffer(const Device& device, GLsizeiptr size, bool vertex_data_usage);
    ~OGLStreamBuffer();

    /*
     * Allocates a linear chunk of memory in the GPU buffer with at least "size" bytes
     * and the optional alignment requirement.
     * If the buffer is full, the whole buffer is reallocated which invalidates old chunks.
     * The return values are the pointer to the new chunk, the offset within the buffer,
     * and the invalidation flag for previous chunks.
     * The actual used size must be specified on unmapping the chunk.
     */
    std::tuple<u8*, GLintptr, bool> Map(GLsizeiptr size, GLintptr alignment = 0);

    void Unmap(GLsizeiptr size);

    GLuint Handle() const {
        return gl_buffer.handle;
    }

    u64 Address() const {
        return gpu_address;
    }

    GLsizeiptr Size() const noexcept {
        return buffer_size;
    }

private:
    OGLBuffer gl_buffer;

    GLuint64EXT gpu_address = 0;
    GLintptr buffer_pos = 0;
    GLsizeiptr buffer_size = 0;
    GLsizeiptr mapped_size = 0;
    u8* mapped_ptr = nullptr;
};

} // namespace OpenGL
