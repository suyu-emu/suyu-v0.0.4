// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include "common/common_types.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_stream_buffer.h"

namespace Core {
class System;
}

namespace OpenGL {

class Device;
class OGLStreamBuffer;
class RasterizerOpenGL;
class StateTracker;

class Buffer : public VideoCommon::BufferBlock {
public:
    explicit Buffer(const Device& device_, VAddr cpu_addr_, std::size_t size_);
    ~Buffer();

    void Upload(std::size_t offset, std::size_t data_size, const u8* data);

    void Download(std::size_t offset, std::size_t data_size, u8* data);

    void CopyFrom(const Buffer& src, std::size_t src_offset, std::size_t dst_offset,
                  std::size_t copy_size);

    GLuint Handle() const noexcept {
        return gl_buffer.handle;
    }

    u64 Address() const noexcept {
        return gpu_address;
    }

private:
    OGLBuffer gl_buffer;
    OGLBuffer read_buffer;
    u64 gpu_address = 0;
};

using GenericBufferCache = VideoCommon::BufferCache<Buffer, GLuint, OGLStreamBuffer>;
class OGLBufferCache final : public GenericBufferCache {
public:
    explicit OGLBufferCache(VideoCore::RasterizerInterface& rasterizer,
                            Tegra::MemoryManager& gpu_memory, Core::Memory::Memory& cpu_memory,
                            const Device& device, OGLStreamBuffer& stream_buffer,
                            StateTracker& state_tracker);
    ~OGLBufferCache();

    BufferInfo GetEmptyBuffer(std::size_t) override;

    void Acquire() noexcept {
        cbuf_cursor = 0;
    }

protected:
    std::shared_ptr<Buffer> CreateBlock(VAddr cpu_addr, std::size_t size) override;

    BufferInfo ConstBufferUpload(const void* raw_pointer, std::size_t size) override;

private:
    static constexpr std::size_t NUM_CBUFS = Tegra::Engines::Maxwell3D::Regs::MaxConstBuffers *
                                             Tegra::Engines::Maxwell3D::Regs::MaxShaderProgram;

    const Device& device;

    std::size_t cbuf_cursor = 0;
    std::array<GLuint, NUM_CBUFS> cbufs{};
};

} // namespace OpenGL
