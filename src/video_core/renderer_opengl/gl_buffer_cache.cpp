// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/microprofile.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

MICROPROFILE_DEFINE(OpenGL_Buffer_Download, "OpenGL", "Buffer Download", MP_RGB(192, 192, 128));

Buffer::Buffer(const Device& device, VAddr cpu_addr, std::size_t size)
    : VideoCommon::BufferBlock{cpu_addr, size} {
    gl_buffer.Create();
    glNamedBufferData(gl_buffer.handle, static_cast<GLsizeiptr>(size), nullptr, GL_DYNAMIC_DRAW);
    if (device.HasVertexBufferUnifiedMemory()) {
        glMakeNamedBufferResidentNV(gl_buffer.handle, GL_READ_WRITE);
        glGetNamedBufferParameterui64vNV(gl_buffer.handle, GL_BUFFER_GPU_ADDRESS_NV, &gpu_address);
    }
}

Buffer::~Buffer() = default;

void Buffer::Upload(std::size_t offset, std::size_t size, const u8* data) const {
    glNamedBufferSubData(Handle(), static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size),
                         data);
}

void Buffer::Download(std::size_t offset, std::size_t size, u8* data) const {
    MICROPROFILE_SCOPE(OpenGL_Buffer_Download);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glGetNamedBufferSubData(Handle(), static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size),
                            data);
}

void Buffer::CopyFrom(const Buffer& src, std::size_t src_offset, std::size_t dst_offset,
                      std::size_t size) const {
    glCopyNamedBufferSubData(src.Handle(), Handle(), static_cast<GLintptr>(src_offset),
                             static_cast<GLintptr>(dst_offset), static_cast<GLsizeiptr>(size));
}

OGLBufferCache::OGLBufferCache(RasterizerOpenGL& rasterizer, Core::System& system,
                               const Device& device_, std::size_t stream_size)
    : GenericBufferCache{rasterizer, system,
                         std::make_unique<OGLStreamBuffer>(device_, stream_size, true)},
      device{device_} {
    if (!device.HasFastBufferSubData()) {
        return;
    }

    static constexpr GLsizeiptr size = static_cast<GLsizeiptr>(Maxwell::MaxConstBufferSize);
    glCreateBuffers(static_cast<GLsizei>(std::size(cbufs)), std::data(cbufs));
    for (const GLuint cbuf : cbufs) {
        glNamedBufferData(cbuf, size, nullptr, GL_STREAM_DRAW);
    }
}

OGLBufferCache::~OGLBufferCache() {
    glDeleteBuffers(static_cast<GLsizei>(std::size(cbufs)), std::data(cbufs));
}

std::shared_ptr<Buffer> OGLBufferCache::CreateBlock(VAddr cpu_addr, std::size_t size) {
    return std::make_shared<Buffer>(device, cpu_addr, size);
}

OGLBufferCache::BufferInfo OGLBufferCache::GetEmptyBuffer(std::size_t) {
    return {0, 0, 0};
}

OGLBufferCache::BufferInfo OGLBufferCache::ConstBufferUpload(const void* raw_pointer,
                                                             std::size_t size) {
    DEBUG_ASSERT(cbuf_cursor < std::size(cbufs));
    const GLuint cbuf = cbufs[cbuf_cursor++];

    glNamedBufferSubData(cbuf, 0, static_cast<GLsizeiptr>(size), raw_pointer);
    return {cbuf, 0, 0};
}

} // namespace OpenGL
