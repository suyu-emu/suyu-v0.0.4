// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/microprofile.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

MICROPROFILE_DEFINE(OpenGL_StreamBuffer, "OpenGL", "Stream Buffer Orphaning",
                    MP_RGB(128, 128, 192));

namespace OpenGL {

OGLStreamBuffer::OGLStreamBuffer(const Device& device, StateTracker& state_tracker_)
    : state_tracker{state_tracker_} {
    gl_buffer.Create();

    static constexpr GLbitfield flags = GL_MAP_WRITE_BIT | GL_MAP_PERSISTENT_BIT;
    glNamedBufferStorage(gl_buffer.handle, BUFFER_SIZE, nullptr, flags);
    mapped_ptr = static_cast<u8*>(
        glMapNamedBufferRange(gl_buffer.handle, 0, BUFFER_SIZE, flags | GL_MAP_FLUSH_EXPLICIT_BIT));

    if (device.UseAssemblyShaders() || device.HasVertexBufferUnifiedMemory()) {
        glMakeNamedBufferResidentNV(gl_buffer.handle, GL_READ_ONLY);
        glGetNamedBufferParameterui64vNV(gl_buffer.handle, GL_BUFFER_GPU_ADDRESS_NV, &gpu_address);
    }
}

OGLStreamBuffer::~OGLStreamBuffer() {
    glUnmapNamedBuffer(gl_buffer.handle);
    gl_buffer.Release();
}

std::pair<u8*, GLintptr> OGLStreamBuffer::Map(GLsizeiptr size, GLintptr alignment) {
    ASSERT(size <= BUFFER_SIZE);
    ASSERT(alignment <= BUFFER_SIZE);
    mapped_size = size;

    if (alignment > 0) {
        buffer_pos = Common::AlignUp<std::size_t>(buffer_pos, alignment);
    }

    if (buffer_pos + size > BUFFER_SIZE) {
        MICROPROFILE_SCOPE(OpenGL_StreamBuffer);
        glInvalidateBufferData(gl_buffer.handle);
        state_tracker.InvalidateStreamBuffer();

        buffer_pos = 0;
    }

    return std::make_pair(mapped_ptr + buffer_pos, buffer_pos);
}

void OGLStreamBuffer::Unmap(GLsizeiptr size) {
    ASSERT(size <= mapped_size);

    if (size > 0) {
        glFlushMappedNamedBufferRange(gl_buffer.handle, buffer_pos, size);
    }

    buffer_pos += size;
}

} // namespace OpenGL
