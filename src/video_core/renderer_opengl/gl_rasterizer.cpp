// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <bitset>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <glad/glad.h>
#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/maxwell_to_gl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

using Tegra::Engines::ShaderType;
using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceType;

MICROPROFILE_DEFINE(OpenGL_VAO, "OpenGL", "Vertex Format Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_VB, "OpenGL", "Vertex Buffer Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Shader, "OpenGL", "Shader Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_UBO, "OpenGL", "Const Buffer Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Index, "OpenGL", "Index Buffer Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Texture, "OpenGL", "Texture Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Framebuffer, "OpenGL", "Framebuffer Setup", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Drawing, "OpenGL", "Drawing", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Blits, "OpenGL", "Blits", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_CacheManagement, "OpenGL", "Cache Mgmt", MP_RGB(100, 255, 100));
MICROPROFILE_DEFINE(OpenGL_PrimitiveAssembly, "OpenGL", "Prim Asmbl", MP_RGB(255, 100, 100));

namespace {

constexpr std::size_t NumSupportedVertexAttributes = 16;

template <typename Engine, typename Entry>
Tegra::Texture::FullTextureInfo GetTextureInfo(const Engine& engine, const Entry& entry,
                                               ShaderType shader_type, std::size_t index = 0) {
    if (entry.IsBindless()) {
        const Tegra::Texture::TextureHandle tex_handle =
            engine.AccessConstBuffer32(shader_type, entry.GetBuffer(), entry.GetOffset());
        return engine.GetTextureInfo(tex_handle);
    }
    const auto& gpu_profile = engine.AccessGuestDriverProfile();
    const u32 offset =
        entry.GetOffset() + static_cast<u32>(index * gpu_profile.GetTextureHandlerSize());
    if constexpr (std::is_same_v<Engine, Tegra::Engines::Maxwell3D>) {
        return engine.GetStageTexture(shader_type, offset);
    } else {
        return engine.GetTexture(offset);
    }
}

std::size_t GetConstBufferSize(const Tegra::Engines::ConstBufferInfo& buffer,
                               const ConstBufferEntry& entry) {
    if (!entry.IsIndirect()) {
        return entry.GetSize();
    }

    if (buffer.size > Maxwell::MaxConstBufferSize) {
        LOG_WARNING(Render_OpenGL, "Indirect constbuffer size {} exceeds maximum {}", buffer.size,
                    Maxwell::MaxConstBufferSize);
        return Maxwell::MaxConstBufferSize;
    }

    return buffer.size;
}

void oglEnable(GLenum cap, bool state) {
    (state ? glEnable : glDisable)(cap);
}

} // Anonymous namespace

RasterizerOpenGL::RasterizerOpenGL(Core::System& system, Core::Frontend::EmuWindow& emu_window,
                                   ScreenInfo& info, GLShader::ProgramManager& program_manager,
                                   StateTracker& state_tracker)
    : RasterizerAccelerated{system.Memory()}, texture_cache{system, *this, device, state_tracker},
      shader_cache{*this, system, emu_window, device}, query_cache{system, *this}, system{system},
      screen_info{info}, program_manager{program_manager}, state_tracker{state_tracker},
      buffer_cache{*this, system, device, STREAM_BUFFER_SIZE} {
    CheckExtensions();
}

RasterizerOpenGL::~RasterizerOpenGL() {}

void RasterizerOpenGL::CheckExtensions() {
    if (!GLAD_GL_ARB_texture_filter_anisotropic && !GLAD_GL_EXT_texture_filter_anisotropic) {
        LOG_WARNING(
            Render_OpenGL,
            "Anisotropic filter is not supported! This can cause graphical issues in some games.");
    }
}

void RasterizerOpenGL::SetupVertexFormat() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::VertexFormats]) {
        return;
    }
    flags[Dirty::VertexFormats] = false;

    MICROPROFILE_SCOPE(OpenGL_VAO);

    // Use the vertex array as-is, assumes that the data is formatted correctly for OpenGL. Enables
    // the first 16 vertex attributes always, as we don't know which ones are actually used until
    // shader time. Note, Tegra technically supports 32, but we're capping this to 16 for now to
    // avoid OpenGL errors.
    // TODO(Subv): Analyze the shader to identify which attributes are actually used and don't
    // assume every shader uses them all.
    for (std::size_t index = 0; index < NumSupportedVertexAttributes; ++index) {
        if (!flags[Dirty::VertexFormat0 + index]) {
            continue;
        }
        flags[Dirty::VertexFormat0 + index] = false;

        const auto attrib = gpu.regs.vertex_attrib_format[index];
        const auto gl_index = static_cast<GLuint>(index);

        // Disable constant attributes.
        if (attrib.IsConstant()) {
            glDisableVertexAttribArray(gl_index);
            continue;
        }
        glEnableVertexAttribArray(gl_index);

        if (attrib.type == Maxwell::VertexAttribute::Type::SignedInt ||
            attrib.type == Maxwell::VertexAttribute::Type::UnsignedInt) {
            glVertexAttribIFormat(gl_index, attrib.ComponentCount(),
                                  MaxwellToGL::VertexType(attrib), attrib.offset);
        } else {
            glVertexAttribFormat(gl_index, attrib.ComponentCount(), MaxwellToGL::VertexType(attrib),
                                 attrib.IsNormalized() ? GL_TRUE : GL_FALSE, attrib.offset);
        }
        glVertexAttribBinding(gl_index, attrib.buffer);
    }
}

void RasterizerOpenGL::SetupVertexBuffer() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::VertexBuffers]) {
        return;
    }
    flags[Dirty::VertexBuffers] = false;

    MICROPROFILE_SCOPE(OpenGL_VB);

    // Upload all guest vertex arrays sequentially to our buffer
    const auto& regs = gpu.regs;
    for (std::size_t index = 0; index < Maxwell::NumVertexArrays; ++index) {
        if (!flags[Dirty::VertexBuffer0 + index]) {
            continue;
        }
        flags[Dirty::VertexBuffer0 + index] = false;

        const auto& vertex_array = regs.vertex_array[index];
        if (!vertex_array.IsEnabled()) {
            continue;
        }

        const GPUVAddr start = vertex_array.StartAddress();
        const GPUVAddr end = regs.vertex_array_limit[index].LimitAddress();

        ASSERT(end > start);
        const u64 size = end - start + 1;
        const auto [vertex_buffer, vertex_buffer_offset] = buffer_cache.UploadMemory(start, size);
        glBindVertexBuffer(static_cast<GLuint>(index), vertex_buffer, vertex_buffer_offset,
                           vertex_array.stride);
    }
}

void RasterizerOpenGL::SetupVertexInstances() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::VertexInstances]) {
        return;
    }
    flags[Dirty::VertexInstances] = false;

    const auto& regs = gpu.regs;
    for (std::size_t index = 0; index < NumSupportedVertexAttributes; ++index) {
        if (!flags[Dirty::VertexInstance0 + index]) {
            continue;
        }
        flags[Dirty::VertexInstance0 + index] = false;

        const auto gl_index = static_cast<GLuint>(index);
        const bool instancing_enabled = regs.instanced_arrays.IsInstancingEnabled(gl_index);
        const GLuint divisor = instancing_enabled ? regs.vertex_array[index].divisor : 0;
        glVertexBindingDivisor(gl_index, divisor);
    }
}

GLintptr RasterizerOpenGL::SetupIndexBuffer() {
    MICROPROFILE_SCOPE(OpenGL_Index);
    const auto& regs = system.GPU().Maxwell3D().regs;
    const std::size_t size = CalculateIndexBufferSize();
    const auto [buffer, offset] = buffer_cache.UploadMemory(regs.index_array.IndexStart(), size);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffer);
    return offset;
}

void RasterizerOpenGL::SetupShaders(GLenum primitive_mode) {
    MICROPROFILE_SCOPE(OpenGL_Shader);
    auto& gpu = system.GPU().Maxwell3D();
    u32 clip_distances = 0;

    for (std::size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        const auto& shader_config = gpu.regs.shader_config[index];
        const auto program{static_cast<Maxwell::ShaderProgram>(index)};

        // Skip stages that are not enabled
        if (!gpu.regs.IsShaderConfigEnabled(index)) {
            switch (program) {
            case Maxwell::ShaderProgram::Geometry:
                program_manager.UseGeometryShader(0);
                break;
            case Maxwell::ShaderProgram::Fragment:
                program_manager.UseFragmentShader(0);
                break;
            default:
                break;
            }
            continue;
        }

        // Currently this stages are not supported in the OpenGL backend.
        // Todo(Blinkhawk): Port tesselation shaders from Vulkan to OpenGL
        if (program == Maxwell::ShaderProgram::TesselationControl) {
            continue;
        } else if (program == Maxwell::ShaderProgram::TesselationEval) {
            continue;
        }

        Shader shader{shader_cache.GetStageProgram(program)};

        // Stage indices are 0 - 5
        const std::size_t stage = index == 0 ? 0 : index - 1;
        SetupDrawConstBuffers(stage, shader);
        SetupDrawGlobalMemory(stage, shader);
        SetupDrawTextures(stage, shader);
        SetupDrawImages(stage, shader);

        const GLuint program_handle = shader->GetHandle();
        switch (program) {
        case Maxwell::ShaderProgram::VertexA:
        case Maxwell::ShaderProgram::VertexB:
            program_manager.UseVertexShader(program_handle);
            break;
        case Maxwell::ShaderProgram::Geometry:
            program_manager.UseGeometryShader(program_handle);
            break;
        case Maxwell::ShaderProgram::Fragment:
            program_manager.UseFragmentShader(program_handle);
            break;
        default:
            UNIMPLEMENTED_MSG("Unimplemented shader index={}, enable={}, offset=0x{:08X}", index,
                              shader_config.enable.Value(), shader_config.offset);
        }

        // Workaround for Intel drivers.
        // When a clip distance is enabled but not set in the shader it crops parts of the screen
        // (sometimes it's half the screen, sometimes three quarters). To avoid this, enable the
        // clip distances only when it's written by a shader stage.
        clip_distances |= shader->GetEntries().clip_distances;

        // When VertexA is enabled, we have dual vertex shaders
        if (program == Maxwell::ShaderProgram::VertexA) {
            // VertexB was combined with VertexA, so we skip the VertexB iteration
            ++index;
        }
    }

    SyncClipEnabled(clip_distances);
    gpu.dirty.flags[Dirty::Shaders] = false;
}

std::size_t RasterizerOpenGL::CalculateVertexArraysSize() const {
    const auto& regs = system.GPU().Maxwell3D().regs;

    std::size_t size = 0;
    for (u32 index = 0; index < Maxwell::NumVertexArrays; ++index) {
        if (!regs.vertex_array[index].IsEnabled())
            continue;

        const GPUVAddr start = regs.vertex_array[index].StartAddress();
        const GPUVAddr end = regs.vertex_array_limit[index].LimitAddress();

        ASSERT(end > start);
        size += end - start + 1;
    }

    return size;
}

std::size_t RasterizerOpenGL::CalculateIndexBufferSize() const {
    const auto& regs = system.GPU().Maxwell3D().regs;

    return static_cast<std::size_t>(regs.index_array.count) *
           static_cast<std::size_t>(regs.index_array.FormatSizeInBytes());
}

void RasterizerOpenGL::LoadDiskResources(const std::atomic_bool& stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    shader_cache.LoadDiskCache(stop_loading, callback);
}

void RasterizerOpenGL::SetupDirtyFlags() {
    state_tracker.Initialize();
}

void RasterizerOpenGL::ConfigureFramebuffers() {
    MICROPROFILE_SCOPE(OpenGL_Framebuffer);
    auto& gpu = system.GPU().Maxwell3D();
    if (!gpu.dirty.flags[VideoCommon::Dirty::RenderTargets]) {
        return;
    }
    gpu.dirty.flags[VideoCommon::Dirty::RenderTargets] = false;

    texture_cache.GuardRenderTargets(true);

    View depth_surface = texture_cache.GetDepthBufferSurface();

    const auto& regs = gpu.regs;
    UNIMPLEMENTED_IF(regs.rt_separate_frag_data == 0);

    // Bind the framebuffer surfaces
    FramebufferCacheKey key;
    const auto colors_count = static_cast<std::size_t>(regs.rt_control.count);
    for (std::size_t index = 0; index < colors_count; ++index) {
        View color_surface{texture_cache.GetColorBufferSurface(index)};
        if (!color_surface) {
            continue;
        }
        // Assume that a surface will be written to if it is used as a framebuffer, even
        // if the shader doesn't actually write to it.
        texture_cache.MarkColorBufferInUse(index);

        key.SetAttachment(index, regs.rt_control.GetMap(index));
        key.colors[index] = std::move(color_surface);
    }

    if (depth_surface) {
        // Assume that a surface will be written to if it is used as a framebuffer, even if
        // the shader doesn't actually write to it.
        texture_cache.MarkDepthBufferInUse();
        key.zeta = std::move(depth_surface);
    }

    texture_cache.GuardRenderTargets(false);

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_cache.GetFramebuffer(key));
}

void RasterizerOpenGL::ConfigureClearFramebuffer(bool using_color_fb, bool using_depth_fb,
                                                 bool using_stencil_fb) {
    auto& gpu = system.GPU().Maxwell3D();
    const auto& regs = gpu.regs;

    texture_cache.GuardRenderTargets(true);
    View color_surface;
    if (using_color_fb) {
        const std::size_t index = regs.clear_buffers.RT;
        color_surface = texture_cache.GetColorBufferSurface(index);
        texture_cache.MarkColorBufferInUse(index);
    }
    View depth_surface;
    if (using_depth_fb || using_stencil_fb) {
        depth_surface = texture_cache.GetDepthBufferSurface();
        texture_cache.MarkDepthBufferInUse();
    }
    texture_cache.GuardRenderTargets(false);

    FramebufferCacheKey key;
    key.colors[0] = color_surface;
    key.zeta = depth_surface;

    state_tracker.NotifyFramebuffer();
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuffer_cache.GetFramebuffer(key));
}

void RasterizerOpenGL::Clear() {
    const auto& gpu = system.GPU().Maxwell3D();
    if (!gpu.ShouldExecute()) {
        return;
    }

    const auto& regs = gpu.regs;
    bool use_color{};
    bool use_depth{};
    bool use_stencil{};

    if (regs.clear_buffers.R || regs.clear_buffers.G || regs.clear_buffers.B ||
        regs.clear_buffers.A) {
        use_color = true;
    }
    if (use_color) {
        state_tracker.NotifyColorMask0();
        glColorMaski(0, regs.clear_buffers.R != 0, regs.clear_buffers.G != 0,
                     regs.clear_buffers.B != 0, regs.clear_buffers.A != 0);

        // TODO(Rodrigo): Determine if clamping is used on clears
        SyncFragmentColorClampState();
        SyncFramebufferSRGB();
    }
    if (regs.clear_buffers.Z) {
        ASSERT_MSG(regs.zeta_enable != 0, "Tried to clear Z but buffer is not enabled!");
        use_depth = true;

        state_tracker.NotifyDepthMask();
        glDepthMask(GL_TRUE);
    }
    if (regs.clear_buffers.S) {
        ASSERT_MSG(regs.zeta_enable, "Tried to clear stencil but buffer is not enabled!");
        use_stencil = true;
    }

    if (!use_color && !use_depth && !use_stencil) {
        // No color surface nor depth/stencil surface are enabled
        return;
    }

    SyncRasterizeEnable();
    SyncStencilTestState();

    if (regs.clear_flags.scissor) {
        SyncScissorTest();
    } else {
        state_tracker.NotifyScissor0();
        glDisablei(GL_SCISSOR_TEST, 0);
    }

    UNIMPLEMENTED_IF(regs.clear_flags.viewport);

    ConfigureClearFramebuffer(use_color, use_depth, use_stencil);

    if (use_color) {
        glClearBufferfv(GL_COLOR, 0, regs.clear_color);
    }

    if (use_depth && use_stencil) {
        glClearBufferfi(GL_DEPTH_STENCIL, 0, regs.clear_depth, regs.clear_stencil);
    } else if (use_depth) {
        glClearBufferfv(GL_DEPTH, 0, &regs.clear_depth);
    } else if (use_stencil) {
        glClearBufferiv(GL_STENCIL, 0, &regs.clear_stencil);
    }

    ++num_queued_commands;
}

void RasterizerOpenGL::Draw(bool is_indexed, bool is_instanced) {
    MICROPROFILE_SCOPE(OpenGL_Drawing);
    auto& gpu = system.GPU().Maxwell3D();

    query_cache.UpdateCounters();

    SyncViewport();
    SyncRasterizeEnable();
    SyncPolygonModes();
    SyncColorMask();
    SyncFragmentColorClampState();
    SyncMultiSampleState();
    SyncDepthTestState();
    SyncDepthClamp();
    SyncStencilTestState();
    SyncBlendState();
    SyncLogicOpState();
    SyncCullMode();
    SyncPrimitiveRestart();
    SyncScissorTest();
    SyncPointState();
    SyncLineState();
    SyncPolygonOffset();
    SyncAlphaTest();
    SyncFramebufferSRGB();

    buffer_cache.Acquire();

    std::size_t buffer_size = CalculateVertexArraysSize();

    // Add space for index buffer
    if (is_indexed) {
        buffer_size = Common::AlignUp(buffer_size, 4) + CalculateIndexBufferSize();
    }

    // Uniform space for the 5 shader stages
    buffer_size = Common::AlignUp<std::size_t>(buffer_size, 4) +
                  (sizeof(GLShader::MaxwellUniformData) + device.GetUniformBufferAlignment()) *
                      Maxwell::MaxShaderStage;

    // Add space for at least 18 constant buffers
    buffer_size += Maxwell::MaxConstBuffers *
                   (Maxwell::MaxConstBufferSize + device.GetUniformBufferAlignment());

    // Prepare the vertex array.
    buffer_cache.Map(buffer_size);

    // Prepare vertex array format.
    SetupVertexFormat();

    // Upload vertex and index data.
    SetupVertexBuffer();
    SetupVertexInstances();
    GLintptr index_buffer_offset = 0;
    if (is_indexed) {
        index_buffer_offset = SetupIndexBuffer();
    }

    // Setup emulation uniform buffer.
    GLShader::MaxwellUniformData ubo;
    ubo.SetFromRegs(gpu);
    const auto [buffer, offset] =
        buffer_cache.UploadHostMemory(&ubo, sizeof(ubo), device.GetUniformBufferAlignment());
    glBindBufferRange(GL_UNIFORM_BUFFER, EmulationUniformBlockBinding, buffer, offset,
                      static_cast<GLsizeiptr>(sizeof(ubo)));

    // Setup shaders and their used resources.
    texture_cache.GuardSamplers(true);
    const GLenum primitive_mode = MaxwellToGL::PrimitiveTopology(gpu.regs.draw.topology);
    SetupShaders(primitive_mode);
    texture_cache.GuardSamplers(false);

    ConfigureFramebuffers();

    // Signal the buffer cache that we are not going to upload more things.
    buffer_cache.Unmap();

    program_manager.BindGraphicsPipeline();

    if (texture_cache.TextureBarrier()) {
        glTextureBarrier();
    }

    BeginTransformFeedback(primitive_mode);

    const GLuint base_instance = static_cast<GLuint>(gpu.regs.vb_base_instance);
    const GLsizei num_instances =
        static_cast<GLsizei>(is_instanced ? gpu.mme_draw.instance_count : 1);
    if (is_indexed) {
        const GLint base_vertex = static_cast<GLint>(gpu.regs.vb_element_base);
        const GLsizei num_vertices = static_cast<GLsizei>(gpu.regs.index_array.count);
        const GLvoid* offset = reinterpret_cast<const GLvoid*>(index_buffer_offset);
        const GLenum format = MaxwellToGL::IndexFormat(gpu.regs.index_array.format);
        if (num_instances == 1 && base_instance == 0 && base_vertex == 0) {
            glDrawElements(primitive_mode, num_vertices, format, offset);
        } else if (num_instances == 1 && base_instance == 0) {
            glDrawElementsBaseVertex(primitive_mode, num_vertices, format, offset, base_vertex);
        } else if (base_vertex == 0 && base_instance == 0) {
            glDrawElementsInstanced(primitive_mode, num_vertices, format, offset, num_instances);
        } else if (base_vertex == 0) {
            glDrawElementsInstancedBaseInstance(primitive_mode, num_vertices, format, offset,
                                                num_instances, base_instance);
        } else if (base_instance == 0) {
            glDrawElementsInstancedBaseVertex(primitive_mode, num_vertices, format, offset,
                                              num_instances, base_vertex);
        } else {
            glDrawElementsInstancedBaseVertexBaseInstance(primitive_mode, num_vertices, format,
                                                          offset, num_instances, base_vertex,
                                                          base_instance);
        }
    } else {
        const GLint base_vertex = static_cast<GLint>(gpu.regs.vertex_buffer.first);
        const GLsizei num_vertices = static_cast<GLsizei>(gpu.regs.vertex_buffer.count);
        if (num_instances == 1 && base_instance == 0) {
            glDrawArrays(primitive_mode, base_vertex, num_vertices);
        } else if (base_instance == 0) {
            glDrawArraysInstanced(primitive_mode, base_vertex, num_vertices, num_instances);
        } else {
            glDrawArraysInstancedBaseInstance(primitive_mode, base_vertex, num_vertices,
                                              num_instances, base_instance);
        }
    }

    EndTransformFeedback();

    ++num_queued_commands;
}

void RasterizerOpenGL::DispatchCompute(GPUVAddr code_addr) {
    if (device.HasBrokenCompute()) {
        return;
    }

    buffer_cache.Acquire();

    auto kernel = shader_cache.GetComputeKernel(code_addr);
    SetupComputeTextures(kernel);
    SetupComputeImages(kernel);
    program_manager.BindComputeShader(kernel->GetHandle());

    const std::size_t buffer_size =
        Tegra::Engines::KeplerCompute::NumConstBuffers *
        (Maxwell::MaxConstBufferSize + device.GetUniformBufferAlignment());
    buffer_cache.Map(buffer_size);

    SetupComputeConstBuffers(kernel);
    SetupComputeGlobalMemory(kernel);

    buffer_cache.Unmap();

    const auto& launch_desc = system.GPU().KeplerCompute().launch_description;
    glDispatchCompute(launch_desc.grid_dim_x, launch_desc.grid_dim_y, launch_desc.grid_dim_z);
    ++num_queued_commands;
}

void RasterizerOpenGL::ResetCounter(VideoCore::QueryType type) {
    query_cache.ResetCounter(type);
}

void RasterizerOpenGL::Query(GPUVAddr gpu_addr, VideoCore::QueryType type,
                             std::optional<u64> timestamp) {
    query_cache.Query(gpu_addr, type, timestamp);
}

void RasterizerOpenGL::FlushAll() {}

void RasterizerOpenGL::FlushRegion(VAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    texture_cache.FlushRegion(addr, size);
    buffer_cache.FlushRegion(addr, size);
    query_cache.FlushRegion(addr, size);
}

void RasterizerOpenGL::InvalidateRegion(VAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    texture_cache.InvalidateRegion(addr, size);
    shader_cache.InvalidateRegion(addr, size);
    buffer_cache.InvalidateRegion(addr, size);
    query_cache.InvalidateRegion(addr, size);
}

void RasterizerOpenGL::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    if (Settings::values.use_accurate_gpu_emulation) {
        FlushRegion(addr, size);
    }
    InvalidateRegion(addr, size);
}

void RasterizerOpenGL::FlushCommands() {
    // Only flush when we have commands queued to OpenGL.
    if (num_queued_commands == 0) {
        return;
    }
    num_queued_commands = 0;
    glFlush();
}

void RasterizerOpenGL::TickFrame() {
    // Ticking a frame means that buffers will be swapped, calling glFlush implicitly.
    num_queued_commands = 0;

    buffer_cache.TickFrame();
}

bool RasterizerOpenGL::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Regs::Surface& src,
                                             const Tegra::Engines::Fermi2D::Regs::Surface& dst,
                                             const Tegra::Engines::Fermi2D::Config& copy_config) {
    MICROPROFILE_SCOPE(OpenGL_Blits);
    texture_cache.DoFermiCopy(src, dst, copy_config);
    return true;
}

bool RasterizerOpenGL::AccelerateDisplay(const Tegra::FramebufferConfig& config,
                                         VAddr framebuffer_addr, u32 pixel_stride) {
    if (!framebuffer_addr) {
        return {};
    }

    MICROPROFILE_SCOPE(OpenGL_CacheManagement);

    const auto surface{texture_cache.TryFindFramebufferSurface(framebuffer_addr)};
    if (!surface) {
        return {};
    }

    // Verify that the cached surface is the same size and format as the requested framebuffer
    const auto& params{surface->GetSurfaceParams()};
    const auto& pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(config.pixel_format)};
    ASSERT_MSG(params.width == config.width, "Framebuffer width is different");
    ASSERT_MSG(params.height == config.height, "Framebuffer height is different");

    if (params.pixel_format != pixel_format) {
        LOG_DEBUG(Render_OpenGL, "Framebuffer pixel_format is different");
    }

    screen_info.display_texture = surface->GetTexture();
    screen_info.display_srgb = surface->GetSurfaceParams().srgb_conversion;

    return true;
}

void RasterizerOpenGL::SetupDrawConstBuffers(std::size_t stage_index, const Shader& shader) {
    MICROPROFILE_SCOPE(OpenGL_UBO);
    const auto& stages = system.GPU().Maxwell3D().state.shader_stages;
    const auto& shader_stage = stages[stage_index];

    u32 binding = device.GetBaseBindings(stage_index).uniform_buffer;
    for (const auto& entry : shader->GetEntries().const_buffers) {
        const auto& buffer = shader_stage.const_buffers[entry.GetIndex()];
        SetupConstBuffer(binding++, buffer, entry);
    }
}

void RasterizerOpenGL::SetupComputeConstBuffers(const Shader& kernel) {
    MICROPROFILE_SCOPE(OpenGL_UBO);
    const auto& launch_desc = system.GPU().KeplerCompute().launch_description;

    u32 binding = 0;
    for (const auto& entry : kernel->GetEntries().const_buffers) {
        const auto& config = launch_desc.const_buffer_config[entry.GetIndex()];
        const std::bitset<8> mask = launch_desc.const_buffer_enable_mask.Value();
        Tegra::Engines::ConstBufferInfo buffer;
        buffer.address = config.Address();
        buffer.size = config.size;
        buffer.enabled = mask[entry.GetIndex()];
        SetupConstBuffer(binding++, buffer, entry);
    }
}

void RasterizerOpenGL::SetupConstBuffer(u32 binding, const Tegra::Engines::ConstBufferInfo& buffer,
                                        const ConstBufferEntry& entry) {
    if (!buffer.enabled) {
        // Set values to zero to unbind buffers
        glBindBufferRange(GL_UNIFORM_BUFFER, binding, buffer_cache.GetEmptyBuffer(sizeof(float)), 0,
                          sizeof(float));
        return;
    }

    // Align the actual size so it ends up being a multiple of vec4 to meet the OpenGL std140
    // UBO alignment requirements.
    const std::size_t size = Common::AlignUp(GetConstBufferSize(buffer, entry), sizeof(GLvec4));

    const auto alignment = device.GetUniformBufferAlignment();
    const auto [cbuf, offset] = buffer_cache.UploadMemory(buffer.address, size, alignment, false,
                                                          device.HasFastBufferSubData());
    glBindBufferRange(GL_UNIFORM_BUFFER, binding, cbuf, offset, size);
}

void RasterizerOpenGL::SetupDrawGlobalMemory(std::size_t stage_index, const Shader& shader) {
    auto& gpu{system.GPU()};
    auto& memory_manager{gpu.MemoryManager()};
    const auto cbufs{gpu.Maxwell3D().state.shader_stages[stage_index]};

    u32 binding = device.GetBaseBindings(stage_index).shader_storage_buffer;
    for (const auto& entry : shader->GetEntries().global_memory_entries) {
        const auto addr{cbufs.const_buffers[entry.GetCbufIndex()].address + entry.GetCbufOffset()};
        const auto gpu_addr{memory_manager.Read<u64>(addr)};
        const auto size{memory_manager.Read<u32>(addr + 8)};
        SetupGlobalMemory(binding++, entry, gpu_addr, size);
    }
}

void RasterizerOpenGL::SetupComputeGlobalMemory(const Shader& kernel) {
    auto& gpu{system.GPU()};
    auto& memory_manager{gpu.MemoryManager()};
    const auto cbufs{gpu.KeplerCompute().launch_description.const_buffer_config};

    u32 binding = 0;
    for (const auto& entry : kernel->GetEntries().global_memory_entries) {
        const auto addr{cbufs[entry.GetCbufIndex()].Address() + entry.GetCbufOffset()};
        const auto gpu_addr{memory_manager.Read<u64>(addr)};
        const auto size{memory_manager.Read<u32>(addr + 8)};
        SetupGlobalMemory(binding++, entry, gpu_addr, size);
    }
}

void RasterizerOpenGL::SetupGlobalMemory(u32 binding, const GlobalMemoryEntry& entry,
                                         GPUVAddr gpu_addr, std::size_t size) {
    const auto alignment{device.GetShaderStorageBufferAlignment()};
    const auto [ssbo, buffer_offset] =
        buffer_cache.UploadMemory(gpu_addr, size, alignment, entry.IsWritten());
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, ssbo, buffer_offset,
                      static_cast<GLsizeiptr>(size));
}

void RasterizerOpenGL::SetupDrawTextures(std::size_t stage_index, const Shader& shader) {
    MICROPROFILE_SCOPE(OpenGL_Texture);
    const auto& maxwell3d = system.GPU().Maxwell3D();
    u32 binding = device.GetBaseBindings(stage_index).sampler;
    for (const auto& entry : shader->GetEntries().samplers) {
        const auto shader_type = static_cast<ShaderType>(stage_index);
        for (std::size_t i = 0; i < entry.Size(); ++i) {
            const auto texture = GetTextureInfo(maxwell3d, entry, shader_type, i);
            SetupTexture(binding++, texture, entry);
        }
    }
}

void RasterizerOpenGL::SetupComputeTextures(const Shader& kernel) {
    MICROPROFILE_SCOPE(OpenGL_Texture);
    const auto& compute = system.GPU().KeplerCompute();
    u32 binding = 0;
    for (const auto& entry : kernel->GetEntries().samplers) {
        for (std::size_t i = 0; i < entry.Size(); ++i) {
            const auto texture = GetTextureInfo(compute, entry, ShaderType::Compute, i);
            SetupTexture(binding++, texture, entry);
        }
    }
}

void RasterizerOpenGL::SetupTexture(u32 binding, const Tegra::Texture::FullTextureInfo& texture,
                                    const SamplerEntry& entry) {
    const auto view = texture_cache.GetTextureSurface(texture.tic, entry);
    if (!view) {
        // Can occur when texture addr is null or its memory is unmapped/invalid
        glBindSampler(binding, 0);
        glBindTextureUnit(binding, 0);
        return;
    }
    glBindTextureUnit(binding, view->GetTexture());

    if (view->GetSurfaceParams().IsBuffer()) {
        return;
    }
    // Apply swizzle to textures that are not buffers.
    view->ApplySwizzle(texture.tic.x_source, texture.tic.y_source, texture.tic.z_source,
                       texture.tic.w_source);

    glBindSampler(binding, sampler_cache.GetSampler(texture.tsc));
}

void RasterizerOpenGL::SetupDrawImages(std::size_t stage_index, const Shader& shader) {
    const auto& maxwell3d = system.GPU().Maxwell3D();
    u32 binding = device.GetBaseBindings(stage_index).image;
    for (const auto& entry : shader->GetEntries().images) {
        const auto shader_type = static_cast<Tegra::Engines::ShaderType>(stage_index);
        const auto tic = GetTextureInfo(maxwell3d, entry, shader_type).tic;
        SetupImage(binding++, tic, entry);
    }
}

void RasterizerOpenGL::SetupComputeImages(const Shader& shader) {
    const auto& compute = system.GPU().KeplerCompute();
    u32 binding = 0;
    for (const auto& entry : shader->GetEntries().images) {
        const auto tic = GetTextureInfo(compute, entry, Tegra::Engines::ShaderType::Compute).tic;
        SetupImage(binding++, tic, entry);
    }
}

void RasterizerOpenGL::SetupImage(u32 binding, const Tegra::Texture::TICEntry& tic,
                                  const ImageEntry& entry) {
    const auto view = texture_cache.GetImageSurface(tic, entry);
    if (!view) {
        glBindImageTexture(binding, 0, 0, GL_FALSE, 0, GL_READ_ONLY, GL_R8);
        return;
    }
    if (!tic.IsBuffer()) {
        view->ApplySwizzle(tic.x_source, tic.y_source, tic.z_source, tic.w_source);
    }
    if (entry.IsWritten()) {
        view->MarkAsModified(texture_cache.Tick());
    }
    glBindImageTexture(binding, view->GetTexture(), 0, GL_TRUE, 0, GL_READ_WRITE,
                       view->GetFormat());
}

void RasterizerOpenGL::SyncViewport() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    const auto& regs = gpu.regs;

    const bool dirty_viewport = flags[Dirty::Viewports];
    if (dirty_viewport || flags[Dirty::ClipControl]) {
        flags[Dirty::ClipControl] = false;

        bool flip_y = false;
        if (regs.viewport_transform[0].scale_y < 0.0) {
            flip_y = !flip_y;
        }
        if (regs.screen_y_control.y_negate != 0) {
            flip_y = !flip_y;
        }
        glClipControl(flip_y ? GL_UPPER_LEFT : GL_LOWER_LEFT,
                      regs.depth_mode == Maxwell::DepthMode::ZeroToOne ? GL_ZERO_TO_ONE
                                                                       : GL_NEGATIVE_ONE_TO_ONE);
    }

    if (dirty_viewport) {
        flags[Dirty::Viewports] = false;

        const bool force = flags[Dirty::ViewportTransform];
        flags[Dirty::ViewportTransform] = false;

        for (std::size_t i = 0; i < Maxwell::NumViewports; ++i) {
            if (!force && !flags[Dirty::Viewport0 + i]) {
                continue;
            }
            flags[Dirty::Viewport0 + i] = false;

            const auto& src = regs.viewport_transform[i];
            const Common::Rectangle<f32> rect{src.GetRect()};
            glViewportIndexedf(static_cast<GLuint>(i), rect.left, rect.bottom, rect.GetWidth(),
                               rect.GetHeight());

            const GLdouble reduce_z = regs.depth_mode == Maxwell::DepthMode::MinusOneToOne;
            const GLdouble near_depth = src.translate_z - src.scale_z * reduce_z;
            const GLdouble far_depth = src.translate_z + src.scale_z;
            glDepthRangeIndexed(static_cast<GLuint>(i), near_depth, far_depth);
        }
    }
}

void RasterizerOpenGL::SyncDepthClamp() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::DepthClampEnabled]) {
        return;
    }
    flags[Dirty::DepthClampEnabled] = false;

    const auto& state = gpu.regs.view_volume_clip_control;
    UNIMPLEMENTED_IF_MSG(state.depth_clamp_far != state.depth_clamp_near,
                         "Unimplemented depth clamp separation!");

    oglEnable(GL_DEPTH_CLAMP, state.depth_clamp_far || state.depth_clamp_near);
}

void RasterizerOpenGL::SyncClipEnabled(u32 clip_mask) {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::ClipDistances] && !flags[Dirty::Shaders]) {
        return;
    }
    flags[Dirty::ClipDistances] = false;

    clip_mask &= gpu.regs.clip_distance_enabled;
    if (clip_mask == last_clip_distance_mask) {
        return;
    }
    last_clip_distance_mask = clip_mask;

    for (std::size_t i = 0; i < Maxwell::Regs::NumClipDistances; ++i) {
        oglEnable(static_cast<GLenum>(GL_CLIP_DISTANCE0 + i), (clip_mask >> i) & 1);
    }
}

void RasterizerOpenGL::SyncClipCoef() {
    UNIMPLEMENTED();
}

void RasterizerOpenGL::SyncCullMode() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    const auto& regs = gpu.regs;

    if (flags[Dirty::CullTest]) {
        flags[Dirty::CullTest] = false;

        if (regs.cull_test_enabled) {
            glEnable(GL_CULL_FACE);
            glCullFace(MaxwellToGL::CullFace(regs.cull_face));
        } else {
            glDisable(GL_CULL_FACE);
        }
    }

    if (flags[Dirty::FrontFace]) {
        flags[Dirty::FrontFace] = false;
        glFrontFace(MaxwellToGL::FrontFace(regs.front_face));
    }
}

void RasterizerOpenGL::SyncPrimitiveRestart() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::PrimitiveRestart]) {
        return;
    }
    flags[Dirty::PrimitiveRestart] = false;

    if (gpu.regs.primitive_restart.enabled) {
        glEnable(GL_PRIMITIVE_RESTART);
        glPrimitiveRestartIndex(gpu.regs.primitive_restart.index);
    } else {
        glDisable(GL_PRIMITIVE_RESTART);
    }
}

void RasterizerOpenGL::SyncDepthTestState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;

    const auto& regs = gpu.regs;
    if (flags[Dirty::DepthMask]) {
        flags[Dirty::DepthMask] = false;
        glDepthMask(regs.depth_write_enabled ? GL_TRUE : GL_FALSE);
    }

    if (flags[Dirty::DepthTest]) {
        flags[Dirty::DepthTest] = false;
        if (regs.depth_test_enable) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(MaxwellToGL::ComparisonOp(regs.depth_test_func));
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }
}

void RasterizerOpenGL::SyncStencilTestState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::StencilTest]) {
        return;
    }
    flags[Dirty::StencilTest] = false;

    const auto& regs = gpu.regs;
    oglEnable(GL_STENCIL_TEST, regs.stencil_enable);

    glStencilFuncSeparate(GL_FRONT, MaxwellToGL::ComparisonOp(regs.stencil_front_func_func),
                          regs.stencil_front_func_ref, regs.stencil_front_func_mask);
    glStencilOpSeparate(GL_FRONT, MaxwellToGL::StencilOp(regs.stencil_front_op_fail),
                        MaxwellToGL::StencilOp(regs.stencil_front_op_zfail),
                        MaxwellToGL::StencilOp(regs.stencil_front_op_zpass));
    glStencilMaskSeparate(GL_FRONT, regs.stencil_front_mask);

    if (regs.stencil_two_side_enable) {
        glStencilFuncSeparate(GL_BACK, MaxwellToGL::ComparisonOp(regs.stencil_back_func_func),
                              regs.stencil_back_func_ref, regs.stencil_back_func_mask);
        glStencilOpSeparate(GL_BACK, MaxwellToGL::StencilOp(regs.stencil_back_op_fail),
                            MaxwellToGL::StencilOp(regs.stencil_back_op_zfail),
                            MaxwellToGL::StencilOp(regs.stencil_back_op_zpass));
        glStencilMaskSeparate(GL_BACK, regs.stencil_back_mask);
    } else {
        glStencilFuncSeparate(GL_BACK, GL_ALWAYS, 0, 0xFFFFFFFF);
        glStencilOpSeparate(GL_BACK, GL_KEEP, GL_KEEP, GL_KEEP);
        glStencilMaskSeparate(GL_BACK, 0xFFFFFFFF);
    }
}

void RasterizerOpenGL::SyncRasterizeEnable() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::RasterizeEnable]) {
        return;
    }
    flags[Dirty::RasterizeEnable] = false;

    oglEnable(GL_RASTERIZER_DISCARD, gpu.regs.rasterize_enable == 0);
}

void RasterizerOpenGL::SyncPolygonModes() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::PolygonModes]) {
        return;
    }
    flags[Dirty::PolygonModes] = false;

    if (gpu.regs.fill_rectangle) {
        if (!GLAD_GL_NV_fill_rectangle) {
            LOG_ERROR(Render_OpenGL, "GL_NV_fill_rectangle used and not supported");
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            return;
        }

        flags[Dirty::PolygonModeFront] = true;
        flags[Dirty::PolygonModeBack] = true;
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL_RECTANGLE_NV);
        return;
    }

    if (gpu.regs.polygon_mode_front == gpu.regs.polygon_mode_back) {
        flags[Dirty::PolygonModeFront] = false;
        flags[Dirty::PolygonModeBack] = false;
        glPolygonMode(GL_FRONT_AND_BACK, MaxwellToGL::PolygonMode(gpu.regs.polygon_mode_front));
        return;
    }

    if (flags[Dirty::PolygonModeFront]) {
        flags[Dirty::PolygonModeFront] = false;
        glPolygonMode(GL_FRONT, MaxwellToGL::PolygonMode(gpu.regs.polygon_mode_front));
    }

    if (flags[Dirty::PolygonModeBack]) {
        flags[Dirty::PolygonModeBack] = false;
        glPolygonMode(GL_BACK, MaxwellToGL::PolygonMode(gpu.regs.polygon_mode_back));
    }
}

void RasterizerOpenGL::SyncColorMask() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::ColorMasks]) {
        return;
    }
    flags[Dirty::ColorMasks] = false;

    const bool force = flags[Dirty::ColorMaskCommon];
    flags[Dirty::ColorMaskCommon] = false;

    const auto& regs = gpu.regs;
    if (regs.color_mask_common) {
        if (!force && !flags[Dirty::ColorMask0]) {
            return;
        }
        flags[Dirty::ColorMask0] = false;

        auto& mask = regs.color_mask[0];
        glColorMask(mask.R != 0, mask.B != 0, mask.G != 0, mask.A != 0);
        return;
    }

    // Path without color_mask_common set
    for (std::size_t i = 0; i < Maxwell::NumRenderTargets; ++i) {
        if (!force && !flags[Dirty::ColorMask0 + i]) {
            continue;
        }
        flags[Dirty::ColorMask0 + i] = false;

        const auto& mask = regs.color_mask[i];
        glColorMaski(static_cast<GLuint>(i), mask.R != 0, mask.G != 0, mask.B != 0, mask.A != 0);
    }
}

void RasterizerOpenGL::SyncMultiSampleState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::MultisampleControl]) {
        return;
    }
    flags[Dirty::MultisampleControl] = false;

    const auto& regs = system.GPU().Maxwell3D().regs;
    oglEnable(GL_SAMPLE_ALPHA_TO_COVERAGE, regs.multisample_control.alpha_to_coverage);
    oglEnable(GL_SAMPLE_ALPHA_TO_ONE, regs.multisample_control.alpha_to_one);
}

void RasterizerOpenGL::SyncFragmentColorClampState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::FragmentClampColor]) {
        return;
    }
    flags[Dirty::FragmentClampColor] = false;

    glClampColor(GL_CLAMP_FRAGMENT_COLOR, gpu.regs.frag_color_clamp ? GL_TRUE : GL_FALSE);
}

void RasterizerOpenGL::SyncBlendState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    const auto& regs = gpu.regs;

    if (flags[Dirty::BlendColor]) {
        flags[Dirty::BlendColor] = false;
        glBlendColor(regs.blend_color.r, regs.blend_color.g, regs.blend_color.b,
                     regs.blend_color.a);
    }

    // TODO(Rodrigo): Revisit blending, there are several registers we are not reading

    if (!flags[Dirty::BlendStates]) {
        return;
    }
    flags[Dirty::BlendStates] = false;

    if (!regs.independent_blend_enable) {
        if (!regs.blend.enable[0]) {
            glDisable(GL_BLEND);
            return;
        }
        glEnable(GL_BLEND);
        glBlendFuncSeparate(MaxwellToGL::BlendFunc(regs.blend.factor_source_rgb),
                            MaxwellToGL::BlendFunc(regs.blend.factor_dest_rgb),
                            MaxwellToGL::BlendFunc(regs.blend.factor_source_a),
                            MaxwellToGL::BlendFunc(regs.blend.factor_dest_a));
        glBlendEquationSeparate(MaxwellToGL::BlendEquation(regs.blend.equation_rgb),
                                MaxwellToGL::BlendEquation(regs.blend.equation_a));
        return;
    }

    const bool force = flags[Dirty::BlendIndependentEnabled];
    flags[Dirty::BlendIndependentEnabled] = false;

    for (std::size_t i = 0; i < Maxwell::NumRenderTargets; ++i) {
        if (!force && !flags[Dirty::BlendState0 + i]) {
            continue;
        }
        flags[Dirty::BlendState0 + i] = false;

        if (!regs.blend.enable[i]) {
            glDisablei(GL_BLEND, static_cast<GLuint>(i));
            continue;
        }
        glEnablei(GL_BLEND, static_cast<GLuint>(i));

        const auto& src = regs.independent_blend[i];
        glBlendFuncSeparatei(static_cast<GLuint>(i), MaxwellToGL::BlendFunc(src.factor_source_rgb),
                             MaxwellToGL::BlendFunc(src.factor_dest_rgb),
                             MaxwellToGL::BlendFunc(src.factor_source_a),
                             MaxwellToGL::BlendFunc(src.factor_dest_a));
        glBlendEquationSeparatei(static_cast<GLuint>(i),
                                 MaxwellToGL::BlendEquation(src.equation_rgb),
                                 MaxwellToGL::BlendEquation(src.equation_a));
    }
}

void RasterizerOpenGL::SyncLogicOpState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::LogicOp]) {
        return;
    }
    flags[Dirty::LogicOp] = false;

    const auto& regs = gpu.regs;
    if (regs.logic_op.enable) {
        glEnable(GL_COLOR_LOGIC_OP);
        glLogicOp(MaxwellToGL::LogicOp(regs.logic_op.operation));
    } else {
        glDisable(GL_COLOR_LOGIC_OP);
    }
}

void RasterizerOpenGL::SyncScissorTest() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::Scissors]) {
        return;
    }
    flags[Dirty::Scissors] = false;

    const auto& regs = gpu.regs;
    for (std::size_t index = 0; index < Maxwell::NumViewports; ++index) {
        if (!flags[Dirty::Scissor0 + index]) {
            continue;
        }
        flags[Dirty::Scissor0 + index] = false;

        const auto& src = regs.scissor_test[index];
        if (src.enable) {
            glEnablei(GL_SCISSOR_TEST, static_cast<GLuint>(index));
            glScissorIndexed(static_cast<GLuint>(index), src.min_x, src.min_y,
                             src.max_x - src.min_x, src.max_y - src.min_y);
        } else {
            glDisablei(GL_SCISSOR_TEST, static_cast<GLuint>(index));
        }
    }
}

void RasterizerOpenGL::SyncPointState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::PointSize]) {
        return;
    }
    flags[Dirty::PointSize] = false;

    oglEnable(GL_POINT_SPRITE, gpu.regs.point_sprite_enable);

    if (gpu.regs.vp_point_size.enable) {
        // By definition of GL_POINT_SIZE, it only matters if GL_PROGRAM_POINT_SIZE is disabled.
        glEnable(GL_PROGRAM_POINT_SIZE);
        return;
    }

    // Limit the point size to 1 since nouveau sometimes sets a point size of 0 (and that's invalid
    // in OpenGL).
    glPointSize(std::max(1.0f, gpu.regs.point_size));
    glDisable(GL_PROGRAM_POINT_SIZE);
}

void RasterizerOpenGL::SyncLineState() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::LineWidth]) {
        return;
    }
    flags[Dirty::LineWidth] = false;

    const auto& regs = gpu.regs;
    oglEnable(GL_LINE_SMOOTH, regs.line_smooth_enable);
    glLineWidth(regs.line_smooth_enable ? regs.line_width_smooth : regs.line_width_aliased);
}

void RasterizerOpenGL::SyncPolygonOffset() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::PolygonOffset]) {
        return;
    }
    flags[Dirty::PolygonOffset] = false;

    const auto& regs = gpu.regs;
    oglEnable(GL_POLYGON_OFFSET_FILL, regs.polygon_offset_fill_enable);
    oglEnable(GL_POLYGON_OFFSET_LINE, regs.polygon_offset_line_enable);
    oglEnable(GL_POLYGON_OFFSET_POINT, regs.polygon_offset_point_enable);

    if (regs.polygon_offset_fill_enable || regs.polygon_offset_line_enable ||
        regs.polygon_offset_point_enable) {
        // Hardware divides polygon offset units by two
        glPolygonOffsetClamp(regs.polygon_offset_factor, regs.polygon_offset_units / 2.0f,
                             regs.polygon_offset_clamp);
    }
}

void RasterizerOpenGL::SyncAlphaTest() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::AlphaTest]) {
        return;
    }
    flags[Dirty::AlphaTest] = false;

    const auto& regs = gpu.regs;
    if (regs.alpha_test_enabled && regs.rt_control.count > 1) {
        LOG_WARNING(Render_OpenGL, "Alpha testing with more than one render target is not tested");
    }

    if (regs.alpha_test_enabled) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(MaxwellToGL::ComparisonOp(regs.alpha_test_func), regs.alpha_test_ref);
    } else {
        glDisable(GL_ALPHA_TEST);
    }
}

void RasterizerOpenGL::SyncFramebufferSRGB() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    if (!flags[Dirty::FramebufferSRGB]) {
        return;
    }
    flags[Dirty::FramebufferSRGB] = false;

    oglEnable(GL_FRAMEBUFFER_SRGB, gpu.regs.framebuffer_srgb);
}

void RasterizerOpenGL::BeginTransformFeedback(GLenum primitive_mode) {
    const auto& regs = system.GPU().Maxwell3D().regs;
    if (regs.tfb_enabled == 0) {
        return;
    }

    UNIMPLEMENTED_IF(regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationControl) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::TesselationEval) ||
                     regs.IsShaderConfigEnabled(Maxwell::ShaderProgram::Geometry));

    for (std::size_t index = 0; index < Maxwell::NumTransformFeedbackBuffers; ++index) {
        const auto& binding = regs.tfb_bindings[index];
        if (!binding.buffer_enable) {
            if (enabled_transform_feedback_buffers[index]) {
                glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(index), 0, 0,
                                  0);
            }
            enabled_transform_feedback_buffers[index] = false;
            continue;
        }
        enabled_transform_feedback_buffers[index] = true;

        auto& tfb_buffer = transform_feedback_buffers[index];
        tfb_buffer.Create();

        const GLuint handle = tfb_buffer.handle;
        const std::size_t size = binding.buffer_size;
        glNamedBufferData(handle, static_cast<GLsizeiptr>(size), nullptr, GL_STREAM_COPY);
        glBindBufferRange(GL_TRANSFORM_FEEDBACK_BUFFER, static_cast<GLuint>(index), handle, 0,
                          static_cast<GLsizeiptr>(size));
    }

    glBeginTransformFeedback(GL_POINTS);
}

void RasterizerOpenGL::EndTransformFeedback() {
    const auto& regs = system.GPU().Maxwell3D().regs;
    if (regs.tfb_enabled == 0) {
        return;
    }

    glEndTransformFeedback();

    for (std::size_t index = 0; index < Maxwell::NumTransformFeedbackBuffers; ++index) {
        const auto& binding = regs.tfb_bindings[index];
        if (!binding.buffer_enable) {
            continue;
        }
        UNIMPLEMENTED_IF(binding.buffer_offset != 0);

        const GLuint handle = transform_feedback_buffers[index].handle;
        const GPUVAddr gpu_addr = binding.Address();
        const std::size_t size = binding.buffer_size;
        const auto [dest_buffer, offset] = buffer_cache.UploadMemory(gpu_addr, size, 4, true);
        glCopyNamedBufferSubData(handle, dest_buffer, 0, offset, static_cast<GLsizeiptr>(size));
    }
}

} // namespace OpenGL
