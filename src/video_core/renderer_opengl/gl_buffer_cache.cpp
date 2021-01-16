// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <span>

#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_instance.h"
#include "video_core/vulkan_common/vulkan_library.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"

namespace OpenGL {
namespace {
struct BindlessSSBO {
    GLuint64EXT address;
    GLsizei length;
    GLsizei padding;
};
static_assert(sizeof(BindlessSSBO) == sizeof(GLuint) * 4);

constexpr std::array PROGRAM_LUT{
    GL_VERTEX_PROGRAM_NV,   GL_TESS_CONTROL_PROGRAM_NV, GL_TESS_EVALUATION_PROGRAM_NV,
    GL_GEOMETRY_PROGRAM_NV, GL_FRAGMENT_PROGRAM_NV,
};
} // Anonymous namespace

Buffer::Buffer(BufferCacheRuntime&, VideoCommon::NullBufferParams null_params)
    : VideoCommon::BufferBase<VideoCore::RasterizerInterface>(null_params) {}

Buffer::Buffer(BufferCacheRuntime& runtime, VideoCore::RasterizerInterface& rasterizer_,
               VAddr cpu_addr_, u64 size_bytes_)
    : VideoCommon::BufferBase<VideoCore::RasterizerInterface>(rasterizer_, cpu_addr_, size_bytes_) {
    buffer.Create();
    const std::string name = fmt::format("Buffer 0x{:x}", CpuAddr());
    glObjectLabel(GL_BUFFER, buffer.handle, static_cast<GLsizei>(name.size()), name.data());
    if (runtime.device.UseAssemblyShaders()) {
        CreateMemoryObjects(runtime);
        glNamedBufferStorageMemEXT(buffer.handle, SizeBytes(), memory_commit.ExportOpenGLHandle(),
                                   memory_commit.Offset());
    } else {
        glNamedBufferData(buffer.handle, SizeBytes(), nullptr, GL_DYNAMIC_DRAW);
    }
    if (runtime.has_unified_vertex_buffers) {
        glGetNamedBufferParameterui64vNV(buffer.handle, GL_BUFFER_GPU_ADDRESS_NV, &address);
    }
}

void Buffer::ImmediateUpload(size_t offset, std::span<const u8> data) noexcept {
    glNamedBufferSubData(buffer.handle, static_cast<GLintptr>(offset),
                         static_cast<GLsizeiptr>(data.size_bytes()), data.data());
}

void Buffer::ImmediateDownload(size_t offset, std::span<u8> data) noexcept {
    glGetNamedBufferSubData(buffer.handle, static_cast<GLintptr>(offset),
                            static_cast<GLsizeiptr>(data.size_bytes()), data.data());
}

void Buffer::MakeResident(GLenum access) noexcept {
    // Abuse GLenum's order to exit early
    // GL_NONE (default) < GL_READ_ONLY < GL_READ_WRITE
    if (access <= current_residency_access || buffer.handle == 0) {
        return;
    }
    if (std::exchange(current_residency_access, access) != GL_NONE) {
        // If the buffer is already resident, remove its residency before promoting it
        glMakeNamedBufferNonResidentNV(buffer.handle);
    }
    glMakeNamedBufferResidentNV(buffer.handle, access);
}

GLuint Buffer::SubBuffer(u32 offset) {
    if (offset == 0) {
        return buffer.handle;
    }
    for (const auto& [sub_buffer, sub_offset] : subs) {
        if (sub_offset == offset) {
            return sub_buffer.handle;
        }
    }
    OGLBuffer sub_buffer;
    sub_buffer.Create();
    glNamedBufferStorageMemEXT(sub_buffer.handle, SizeBytes() - offset,
                               memory_commit.ExportOpenGLHandle(), memory_commit.Offset() + offset);
    return subs.emplace_back(std::move(sub_buffer), offset).first.handle;
}

void Buffer::CreateMemoryObjects(BufferCacheRuntime& runtime) {
    auto& allocator = runtime.vulkan_memory_allocator;
    auto& device = runtime.vulkan_device->GetLogical();
    auto vulkan_buffer = device.CreateBuffer(VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = SizeBytes(),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
    const VkMemoryRequirements requirements = device.GetBufferMemoryRequirements(*vulkan_buffer);
    memory_commit = allocator->Commit(requirements, Vulkan::MemoryUsage::DeviceLocal);
}

BufferCacheRuntime::BufferCacheRuntime(const Device& device_, const Vulkan::Device* vulkan_device_,
                                       Vulkan::MemoryAllocator* vulkan_memory_allocator_)
    : device{device_}, vulkan_device{vulkan_device_},
      vulkan_memory_allocator{vulkan_memory_allocator_},
      stream_buffer{device.HasFastBufferSubData() ? std::nullopt
                                                  : std::make_optional<StreamBuffer>()} {
    GLint gl_max_attributes;
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &gl_max_attributes);
    max_attributes = static_cast<u32>(gl_max_attributes);
    use_assembly_shaders = device.UseAssemblyShaders();
    has_unified_vertex_buffers = device.HasVertexBufferUnifiedMemory();

    for (auto& stage_uniforms : fast_uniforms) {
        for (OGLBuffer& buffer : stage_uniforms) {
            buffer.Create();
            glNamedBufferData(buffer.handle, BufferCache::SKIP_CACHE_SIZE, nullptr, GL_STREAM_DRAW);
        }
    }
}

void BufferCacheRuntime::CopyBuffer(Buffer& dst_buffer, Buffer& src_buffer,
                                    std::span<const VideoCommon::BufferCopy> copies) {
    for (const VideoCommon::BufferCopy& copy : copies) {
        glCopyNamedBufferSubData(
            src_buffer.Handle(), dst_buffer.Handle(), static_cast<GLintptr>(copy.src_offset),
            static_cast<GLintptr>(copy.dst_offset), static_cast<GLsizeiptr>(copy.size));
    }
}

void BufferCacheRuntime::BindIndexBuffer(Buffer& buffer, u32 offset, u32 size) {
    if (has_unified_vertex_buffers) {
        buffer.MakeResident(GL_READ_ONLY);
        glBufferAddressRangeNV(GL_ELEMENT_ARRAY_ADDRESS_NV, 0, buffer.HostGpuAddr() + offset,
                               static_cast<GLsizeiptr>(size));
    } else {
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer.Handle());
        index_buffer_offset = offset;
    }
}

void BufferCacheRuntime::BindVertexBuffer(u32 index, Buffer& buffer, u32 offset, u32 size,
                                          u32 stride) {
    if (index >= max_attributes) {
        return;
    }
    if (has_unified_vertex_buffers) {
        buffer.MakeResident(GL_READ_ONLY);
        glBindVertexBuffer(index, 0, 0, static_cast<GLsizei>(stride));
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, index,
                               buffer.HostGpuAddr() + offset, static_cast<GLsizeiptr>(size));
    } else {
        glBindVertexBuffer(index, buffer.Handle(), static_cast<GLintptr>(offset),
                           static_cast<GLsizei>(stride));
    }
}

void BufferCacheRuntime::BindUniformBuffer(size_t stage, u32 binding_index, Buffer& buffer,
                                           u32 offset, u32 size) {
    if (use_assembly_shaders) {
        const GLuint sub_buffer = buffer.SubBuffer(offset);
        glBindBufferRangeNV(PABO_LUT[stage], binding_index, sub_buffer, 0,
                            static_cast<GLsizeiptr>(size));
    } else {
        const GLuint base_binding = device.GetBaseBindings(stage).uniform_buffer;
        const GLuint binding = base_binding + binding_index;
        glBindBufferRange(GL_UNIFORM_BUFFER, binding, buffer.Handle(),
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }
}

void BufferCacheRuntime::BindComputeUniformBuffer(u32 binding_index, Buffer& buffer, u32 offset,
                                                  u32 size) {
    if (use_assembly_shaders) {
        glBindBufferRangeNV(GL_COMPUTE_PROGRAM_PARAMETER_BUFFER_NV, binding_index,
                            buffer.SubBuffer(offset), 0, static_cast<GLsizeiptr>(size));
    } else {
        glBindBufferRange(GL_UNIFORM_BUFFER, binding_index, buffer.Handle(),
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }
}

void BufferCacheRuntime::BindStorageBuffer(size_t stage, u32 binding_index, Buffer& buffer,
                                           u32 offset, u32 size, bool is_written) {
    if (use_assembly_shaders) {
        const BindlessSSBO ssbo{
            .address = buffer.HostGpuAddr() + offset,
            .length = static_cast<GLsizei>(size),
            .padding = 0,
        };
        buffer.MakeResident(is_written ? GL_READ_WRITE : GL_READ_ONLY);
        glProgramLocalParametersI4uivNV(PROGRAM_LUT[stage], binding_index, 1,
                                        reinterpret_cast<const GLuint*>(&ssbo));
    } else {
        const GLuint base_binding = device.GetBaseBindings(stage).shader_storage_buffer;
        const GLuint binding = base_binding + binding_index;
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, buffer.Handle(),
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }
}

void BufferCacheRuntime::BindComputeStorageBuffer(u32 binding_index, Buffer& buffer, u32 offset,
                                                  u32 size, bool is_written) {
    if (use_assembly_shaders) {
        const BindlessSSBO ssbo{
            .address = buffer.HostGpuAddr() + offset,
            .length = static_cast<GLsizei>(size),
            .padding = 0,
        };
        buffer.MakeResident(is_written ? GL_READ_WRITE : GL_READ_ONLY);
        glProgramLocalParametersI4uivNV(GL_COMPUTE_PROGRAM_NV, binding_index, 1,
                                        reinterpret_cast<const GLuint*>(&ssbo));
    } else if (size == 0) {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding_index, 0, 0, 0);
    } else {
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding_index, buffer.Handle(),
                          static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
    }
}

void BufferCacheRuntime::BindTransformFeedbackBuffer(u32 index, Buffer& buffer, u32 offset,
                                                     u32 size) {
    glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, index, buffer.Handle(),
                      static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
}

} // namespace OpenGL
