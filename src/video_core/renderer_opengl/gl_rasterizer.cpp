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
#include "video_core/shader_cache.h"

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

constexpr std::size_t NUM_CONST_BUFFERS_PER_STAGE = 18;
constexpr std::size_t NUM_CONST_BUFFERS_BYTES_PER_STAGE =
    NUM_CONST_BUFFERS_PER_STAGE * Maxwell::MaxConstBufferSize;
constexpr std::size_t TOTAL_CONST_BUFFER_BYTES =
    NUM_CONST_BUFFERS_BYTES_PER_STAGE * Maxwell::MaxShaderStage;

constexpr std::size_t NUM_SUPPORTED_VERTEX_ATTRIBUTES = 16;
constexpr std::size_t NUM_SUPPORTED_VERTEX_BINDINGS = 16;

template <typename Engine, typename Entry>
Tegra::Texture::FullTextureInfo GetTextureInfo(const Engine& engine, const Entry& entry,
                                               ShaderType shader_type, std::size_t index = 0) {
    if constexpr (std::is_same_v<Entry, SamplerEntry>) {
        if (entry.is_separated) {
            const u32 buffer_1 = entry.buffer;
            const u32 buffer_2 = entry.secondary_buffer;
            const u32 offset_1 = entry.offset;
            const u32 offset_2 = entry.secondary_offset;
            const u32 handle_1 = engine.AccessConstBuffer32(shader_type, buffer_1, offset_1);
            const u32 handle_2 = engine.AccessConstBuffer32(shader_type, buffer_2, offset_2);
            return engine.GetTextureInfo(handle_1 | handle_2);
        }
    }
    if (entry.is_bindless) {
        const u32 handle = engine.AccessConstBuffer32(shader_type, entry.buffer, entry.offset);
        return engine.GetTextureInfo(handle);
    }

    const auto& gpu_profile = engine.AccessGuestDriverProfile();
    const u32 offset = entry.offset + static_cast<u32>(index * gpu_profile.GetTextureHandlerSize());
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

/// Translates hardware transform feedback indices
/// @param location Hardware location
/// @return Pair of ARB_transform_feedback3 token stream first and third arguments
/// @note Read https://www.khronos.org/registry/OpenGL/extensions/ARB/ARB_transform_feedback3.txt
std::pair<GLint, GLint> TransformFeedbackEnum(u8 location) {
    const u8 index = location / 4;
    if (index >= 8 && index <= 39) {
        return {GL_GENERIC_ATTRIB_NV, index - 8};
    }
    if (index >= 48 && index <= 55) {
        return {GL_TEXTURE_COORD_NV, index - 48};
    }
    switch (index) {
    case 7:
        return {GL_POSITION, 0};
    case 40:
        return {GL_PRIMARY_COLOR_NV, 0};
    case 41:
        return {GL_SECONDARY_COLOR_NV, 0};
    case 42:
        return {GL_BACK_PRIMARY_COLOR_NV, 0};
    case 43:
        return {GL_BACK_SECONDARY_COLOR_NV, 0};
    }
    UNIMPLEMENTED_MSG("index={}", static_cast<int>(index));
    return {GL_POSITION, 0};
}

void oglEnable(GLenum cap, bool state) {
    (state ? glEnable : glDisable)(cap);
}

} // Anonymous namespace

RasterizerOpenGL::RasterizerOpenGL(Core::System& system, Core::Frontend::EmuWindow& emu_window,
                                   const Device& device, ScreenInfo& info,
                                   ProgramManager& program_manager, StateTracker& state_tracker)
    : RasterizerAccelerated{system.Memory()}, device{device}, texture_cache{system, *this, device,
                                                                            state_tracker},
      shader_cache{*this, system, emu_window, device}, query_cache{system, *this},
      buffer_cache{*this, system, device, STREAM_BUFFER_SIZE},
      fence_manager{system, *this, texture_cache, buffer_cache, query_cache}, system{system},
      screen_info{info}, program_manager{program_manager}, state_tracker{state_tracker} {
    CheckExtensions();

    unified_uniform_buffer.Create();
    glNamedBufferStorage(unified_uniform_buffer.handle, TOTAL_CONST_BUFFER_BYTES, nullptr, 0);

    if (device.UseAssemblyShaders()) {
        glCreateBuffers(static_cast<GLsizei>(staging_cbufs.size()), staging_cbufs.data());
        for (const GLuint cbuf : staging_cbufs) {
            glNamedBufferStorage(cbuf, static_cast<GLsizeiptr>(Maxwell::MaxConstBufferSize),
                                 nullptr, 0);
        }
    }
}

RasterizerOpenGL::~RasterizerOpenGL() {
    if (device.UseAssemblyShaders()) {
        glDeleteBuffers(static_cast<GLsizei>(staging_cbufs.size()), staging_cbufs.data());
    }
}

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
    for (std::size_t index = 0; index < NUM_SUPPORTED_VERTEX_ATTRIBUTES; ++index) {
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

    const bool use_unified_memory = device.HasVertexBufferUnifiedMemory();

    // Upload all guest vertex arrays sequentially to our buffer
    const auto& regs = gpu.regs;
    for (std::size_t index = 0; index < NUM_SUPPORTED_VERTEX_BINDINGS; ++index) {
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
        ASSERT(end >= start);

        const GLuint gl_index = static_cast<GLuint>(index);
        const u64 size = end - start;
        if (size == 0) {
            glBindVertexBuffer(gl_index, 0, 0, vertex_array.stride);
            if (use_unified_memory) {
                glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, gl_index, 0, 0);
            }
            continue;
        }
        const auto info = buffer_cache.UploadMemory(start, size);
        if (use_unified_memory) {
            glBindVertexBuffer(gl_index, 0, 0, vertex_array.stride);
            glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, gl_index,
                                   info.address + info.offset, size);
        } else {
            glBindVertexBuffer(gl_index, info.handle, info.offset, vertex_array.stride);
        }
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
    for (std::size_t index = 0; index < NUM_SUPPORTED_VERTEX_ATTRIBUTES; ++index) {
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
    const auto info = buffer_cache.UploadMemory(regs.index_array.IndexStart(), size);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, info.handle);
    return info.offset;
}

void RasterizerOpenGL::SetupShaders(GLenum primitive_mode) {
    MICROPROFILE_SCOPE(OpenGL_Shader);
    auto& gpu = system.GPU().Maxwell3D();
    std::size_t num_ssbos = 0;
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

        Shader* const shader = shader_cache.GetStageProgram(program);

        if (device.UseAssemblyShaders()) {
            // Check for ARB limitation. We only have 16 SSBOs per context state. To workaround this
            // all stages share the same bindings.
            const std::size_t num_stage_ssbos = shader->GetEntries().global_memory_entries.size();
            ASSERT_MSG(num_stage_ssbos == 0 || num_ssbos == 0, "SSBOs on more than one stage");
            num_ssbos += num_stage_ssbos;
        }

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

        size += end - start;
        ASSERT(end >= start);
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

    View depth_surface = texture_cache.GetDepthBufferSurface(true);

    const auto& regs = gpu.regs;
    UNIMPLEMENTED_IF(regs.rt_separate_frag_data == 0);

    // Bind the framebuffer surfaces
    FramebufferCacheKey key;
    const auto colors_count = static_cast<std::size_t>(regs.rt_control.count);
    for (std::size_t index = 0; index < colors_count; ++index) {
        View color_surface{texture_cache.GetColorBufferSurface(index, true)};
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

void RasterizerOpenGL::ConfigureClearFramebuffer(bool using_color, bool using_depth_stencil) {
    auto& gpu = system.GPU().Maxwell3D();
    const auto& regs = gpu.regs;

    texture_cache.GuardRenderTargets(true);
    View color_surface;

    if (using_color) {
        // Determine if we have to preserve the contents.
        // First we have to make sure all clear masks are enabled.
        bool preserve_contents = !regs.clear_buffers.R || !regs.clear_buffers.G ||
                                 !regs.clear_buffers.B || !regs.clear_buffers.A;
        const std::size_t index = regs.clear_buffers.RT;
        if (regs.clear_flags.scissor) {
            // Then we have to confirm scissor testing clears the whole image.
            const auto& scissor = regs.scissor_test[0];
            preserve_contents |= scissor.min_x > 0;
            preserve_contents |= scissor.min_y > 0;
            preserve_contents |= scissor.max_x < regs.rt[index].width;
            preserve_contents |= scissor.max_y < regs.rt[index].height;
        }

        color_surface = texture_cache.GetColorBufferSurface(index, preserve_contents);
        texture_cache.MarkColorBufferInUse(index);
    }

    View depth_surface;
    if (using_depth_stencil) {
        bool preserve_contents = false;
        if (regs.clear_flags.scissor) {
            // For depth stencil clears we only have to confirm scissor test covers the whole image.
            const auto& scissor = regs.scissor_test[0];
            preserve_contents |= scissor.min_x > 0;
            preserve_contents |= scissor.min_y > 0;
            preserve_contents |= scissor.max_x < regs.zeta_width;
            preserve_contents |= scissor.max_y < regs.zeta_height;
        }

        depth_surface = texture_cache.GetDepthBufferSurface(preserve_contents);
        texture_cache.MarkDepthBufferInUse();
    }
    texture_cache.GuardRenderTargets(false);

    FramebufferCacheKey key;
    key.colors[0] = std::move(color_surface);
    key.zeta = std::move(depth_surface);

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

    ConfigureClearFramebuffer(use_color, use_depth || use_stencil);

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
    current_cbuf = 0;

    std::size_t buffer_size = CalculateVertexArraysSize();

    // Add space for index buffer
    if (is_indexed) {
        buffer_size = Common::AlignUp(buffer_size, 4) + CalculateIndexBufferSize();
    }

    // Uniform space for the 5 shader stages
    buffer_size =
        Common::AlignUp<std::size_t>(buffer_size, 4) +
        (sizeof(MaxwellUniformData) + device.GetUniformBufferAlignment()) * Maxwell::MaxShaderStage;

    // Add space for at least 18 constant buffers
    buffer_size += Maxwell::MaxConstBuffers *
                   (Maxwell::MaxConstBufferSize + device.GetUniformBufferAlignment());

    // Prepare the vertex array.
    const bool invalidated = buffer_cache.Map(buffer_size);

    if (invalidated) {
        // When the stream buffer has been invalidated, we have to consider vertex buffers as dirty
        auto& dirty = gpu.dirty.flags;
        dirty[Dirty::VertexBuffers] = true;
        for (int index = Dirty::VertexBuffer0; index <= Dirty::VertexBuffer31; ++index) {
            dirty[index] = true;
        }
    }

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
    if (!device.UseAssemblyShaders()) {
        MaxwellUniformData ubo;
        ubo.SetFromRegs(gpu);
        const auto info =
            buffer_cache.UploadHostMemory(&ubo, sizeof(ubo), device.GetUniformBufferAlignment());
        glBindBufferRange(GL_UNIFORM_BUFFER, EmulationUniformBlockBinding, info.handle, info.offset,
                          static_cast<GLsizeiptr>(sizeof(ubo)));
    }

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

    system.GPU().TickWork();
}

void RasterizerOpenGL::DispatchCompute(GPUVAddr code_addr) {
    buffer_cache.Acquire();
    current_cbuf = 0;

    auto kernel = shader_cache.GetComputeKernel(code_addr);
    SetupComputeTextures(kernel);
    SetupComputeImages(kernel);

    const std::size_t buffer_size =
        Tegra::Engines::KeplerCompute::NumConstBuffers *
        (Maxwell::MaxConstBufferSize + device.GetUniformBufferAlignment());
    buffer_cache.Map(buffer_size);

    SetupComputeConstBuffers(kernel);
    SetupComputeGlobalMemory(kernel);

    buffer_cache.Unmap();

    const auto& launch_desc = system.GPU().KeplerCompute().launch_description;
    program_manager.BindCompute(kernel->GetHandle());
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

bool RasterizerOpenGL::MustFlushRegion(VAddr addr, u64 size) {
    if (!Settings::IsGPULevelHigh()) {
        return buffer_cache.MustFlushRegion(addr, size);
    }
    return texture_cache.MustFlushRegion(addr, size) || buffer_cache.MustFlushRegion(addr, size);
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

void RasterizerOpenGL::OnCPUWrite(VAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    texture_cache.OnCPUWrite(addr, size);
    shader_cache.OnCPUWrite(addr, size);
    buffer_cache.OnCPUWrite(addr, size);
}

void RasterizerOpenGL::SyncGuestHost() {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    texture_cache.SyncGuestHost();
    buffer_cache.SyncGuestHost();
    shader_cache.SyncGuestHost();
}

void RasterizerOpenGL::SignalSemaphore(GPUVAddr addr, u32 value) {
    auto& gpu{system.GPU()};
    if (!gpu.IsAsync()) {
        auto& memory_manager{gpu.MemoryManager()};
        memory_manager.Write<u32>(addr, value);
        return;
    }
    fence_manager.SignalSemaphore(addr, value);
}

void RasterizerOpenGL::SignalSyncPoint(u32 value) {
    auto& gpu{system.GPU()};
    if (!gpu.IsAsync()) {
        gpu.IncrementSyncPoint(value);
        return;
    }
    fence_manager.SignalSyncPoint(value);
}

void RasterizerOpenGL::ReleaseFences() {
    auto& gpu{system.GPU()};
    if (!gpu.IsAsync()) {
        return;
    }
    fence_manager.WaitPendingFences();
}

void RasterizerOpenGL::FlushAndInvalidateRegion(VAddr addr, u64 size) {
    if (Settings::IsGPULevelExtreme()) {
        FlushRegion(addr, size);
    }
    InvalidateRegion(addr, size);
}

void RasterizerOpenGL::WaitForIdle() {
    // Place a barrier on everything that is not framebuffer related.
    // This is related to another flag that is not currently implemented.
    glMemoryBarrier(GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT | GL_ELEMENT_ARRAY_BARRIER_BIT |
                    GL_UNIFORM_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT |
                    GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_COMMAND_BARRIER_BIT |
                    GL_PIXEL_BUFFER_BARRIER_BIT | GL_TEXTURE_UPDATE_BARRIER_BIT |
                    GL_BUFFER_UPDATE_BARRIER_BIT | GL_TRANSFORM_FEEDBACK_BARRIER_BIT |
                    GL_SHADER_STORAGE_BARRIER_BIT | GL_QUERY_BUFFER_BARRIER_BIT);
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

void RasterizerOpenGL::SetupDrawConstBuffers(std::size_t stage_index, Shader* shader) {
    static constexpr std::array PARAMETER_LUT = {
        GL_VERTEX_PROGRAM_PARAMETER_BUFFER_NV, GL_TESS_CONTROL_PROGRAM_PARAMETER_BUFFER_NV,
        GL_TESS_EVALUATION_PROGRAM_PARAMETER_BUFFER_NV, GL_GEOMETRY_PROGRAM_PARAMETER_BUFFER_NV,
        GL_FRAGMENT_PROGRAM_PARAMETER_BUFFER_NV};

    MICROPROFILE_SCOPE(OpenGL_UBO);
    const auto& stages = system.GPU().Maxwell3D().state.shader_stages;
    const auto& shader_stage = stages[stage_index];
    const auto& entries = shader->GetEntries();
    const bool use_unified = entries.use_unified_uniforms;
    const std::size_t base_unified_offset = stage_index * NUM_CONST_BUFFERS_BYTES_PER_STAGE;

    const auto base_bindings = device.GetBaseBindings(stage_index);
    u32 binding = device.UseAssemblyShaders() ? 0 : base_bindings.uniform_buffer;
    for (const auto& entry : entries.const_buffers) {
        const u32 index = entry.GetIndex();
        const auto& buffer = shader_stage.const_buffers[index];
        SetupConstBuffer(PARAMETER_LUT[stage_index], binding, buffer, entry, use_unified,
                         base_unified_offset + index * Maxwell::MaxConstBufferSize);
        ++binding;
    }
    if (use_unified) {
        const u32 index = static_cast<u32>(base_bindings.shader_storage_buffer +
                                           entries.global_memory_entries.size());
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, unified_uniform_buffer.handle,
                          base_unified_offset, NUM_CONST_BUFFERS_BYTES_PER_STAGE);
    }
}

void RasterizerOpenGL::SetupComputeConstBuffers(Shader* kernel) {
    MICROPROFILE_SCOPE(OpenGL_UBO);
    const auto& launch_desc = system.GPU().KeplerCompute().launch_description;
    const auto& entries = kernel->GetEntries();
    const bool use_unified = entries.use_unified_uniforms;

    u32 binding = 0;
    for (const auto& entry : entries.const_buffers) {
        const auto& config = launch_desc.const_buffer_config[entry.GetIndex()];
        const std::bitset<8> mask = launch_desc.const_buffer_enable_mask.Value();
        Tegra::Engines::ConstBufferInfo buffer;
        buffer.address = config.Address();
        buffer.size = config.size;
        buffer.enabled = mask[entry.GetIndex()];
        SetupConstBuffer(GL_COMPUTE_PROGRAM_PARAMETER_BUFFER_NV, binding, buffer, entry,
                         use_unified, entry.GetIndex() * Maxwell::MaxConstBufferSize);
        ++binding;
    }
    if (use_unified) {
        const GLuint index = static_cast<GLuint>(entries.global_memory_entries.size());
        glBindBufferRange(GL_SHADER_STORAGE_BUFFER, index, unified_uniform_buffer.handle, 0,
                          NUM_CONST_BUFFERS_BYTES_PER_STAGE);
    }
}

void RasterizerOpenGL::SetupConstBuffer(GLenum stage, u32 binding,
                                        const Tegra::Engines::ConstBufferInfo& buffer,
                                        const ConstBufferEntry& entry, bool use_unified,
                                        std::size_t unified_offset) {
    if (!buffer.enabled) {
        // Set values to zero to unbind buffers
        if (device.UseAssemblyShaders()) {
            glBindBufferRangeNV(stage, entry.GetIndex(), 0, 0, 0);
        } else {
            glBindBufferRange(GL_UNIFORM_BUFFER, binding, 0, 0, sizeof(float));
        }
        return;
    }

    // Align the actual size so it ends up being a multiple of vec4 to meet the OpenGL std140
    // UBO alignment requirements.
    const std::size_t size = Common::AlignUp(GetConstBufferSize(buffer, entry), sizeof(GLvec4));

    const bool fast_upload = !use_unified && device.HasFastBufferSubData();

    const std::size_t alignment = use_unified ? 4 : device.GetUniformBufferAlignment();
    const GPUVAddr gpu_addr = buffer.address;
    auto info = buffer_cache.UploadMemory(gpu_addr, size, alignment, false, fast_upload);

    if (device.UseAssemblyShaders()) {
        UNIMPLEMENTED_IF(use_unified);
        if (info.offset != 0) {
            const GLuint staging_cbuf = staging_cbufs[current_cbuf++];
            glCopyNamedBufferSubData(info.handle, staging_cbuf, info.offset, 0, size);
            info.handle = staging_cbuf;
            info.offset = 0;
        }
        glBindBufferRangeNV(stage, binding, info.handle, info.offset, size);
        return;
    }

    if (use_unified) {
        glCopyNamedBufferSubData(info.handle, unified_uniform_buffer.handle, info.offset,
                                 unified_offset, size);
    } else {
        glBindBufferRange(GL_UNIFORM_BUFFER, binding, info.handle, info.offset, size);
    }
}

void RasterizerOpenGL::SetupDrawGlobalMemory(std::size_t stage_index, Shader* shader) {
    auto& gpu{system.GPU()};
    auto& memory_manager{gpu.MemoryManager()};
    const auto cbufs{gpu.Maxwell3D().state.shader_stages[stage_index]};

    u32 binding =
        device.UseAssemblyShaders() ? 0 : device.GetBaseBindings(stage_index).shader_storage_buffer;
    for (const auto& entry : shader->GetEntries().global_memory_entries) {
        const GPUVAddr addr{cbufs.const_buffers[entry.cbuf_index].address + entry.cbuf_offset};
        const GPUVAddr gpu_addr{memory_manager.Read<u64>(addr)};
        const u32 size{memory_manager.Read<u32>(addr + 8)};
        SetupGlobalMemory(binding++, entry, gpu_addr, size);
    }
}

void RasterizerOpenGL::SetupComputeGlobalMemory(Shader* kernel) {
    auto& gpu{system.GPU()};
    auto& memory_manager{gpu.MemoryManager()};
    const auto cbufs{gpu.KeplerCompute().launch_description.const_buffer_config};

    u32 binding = 0;
    for (const auto& entry : kernel->GetEntries().global_memory_entries) {
        const auto addr{cbufs[entry.cbuf_index].Address() + entry.cbuf_offset};
        const auto gpu_addr{memory_manager.Read<u64>(addr)};
        const auto size{memory_manager.Read<u32>(addr + 8)};
        SetupGlobalMemory(binding++, entry, gpu_addr, size);
    }
}

void RasterizerOpenGL::SetupGlobalMemory(u32 binding, const GlobalMemoryEntry& entry,
                                         GPUVAddr gpu_addr, std::size_t size) {
    const auto alignment{device.GetShaderStorageBufferAlignment()};
    const auto info = buffer_cache.UploadMemory(gpu_addr, size, alignment, entry.is_written);
    glBindBufferRange(GL_SHADER_STORAGE_BUFFER, binding, info.handle, info.offset,
                      static_cast<GLsizeiptr>(size));
}

void RasterizerOpenGL::SetupDrawTextures(std::size_t stage_index, Shader* shader) {
    MICROPROFILE_SCOPE(OpenGL_Texture);
    const auto& maxwell3d = system.GPU().Maxwell3D();
    u32 binding = device.GetBaseBindings(stage_index).sampler;
    for (const auto& entry : shader->GetEntries().samplers) {
        const auto shader_type = static_cast<ShaderType>(stage_index);
        for (std::size_t i = 0; i < entry.size; ++i) {
            const auto texture = GetTextureInfo(maxwell3d, entry, shader_type, i);
            SetupTexture(binding++, texture, entry);
        }
    }
}

void RasterizerOpenGL::SetupComputeTextures(Shader* kernel) {
    MICROPROFILE_SCOPE(OpenGL_Texture);
    const auto& compute = system.GPU().KeplerCompute();
    u32 binding = 0;
    for (const auto& entry : kernel->GetEntries().samplers) {
        for (std::size_t i = 0; i < entry.size; ++i) {
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
    const GLuint handle = view->GetTexture(texture.tic.x_source, texture.tic.y_source,
                                           texture.tic.z_source, texture.tic.w_source);
    glBindTextureUnit(binding, handle);
    if (!view->GetSurfaceParams().IsBuffer()) {
        glBindSampler(binding, sampler_cache.GetSampler(texture.tsc));
    }
}

void RasterizerOpenGL::SetupDrawImages(std::size_t stage_index, Shader* shader) {
    const auto& maxwell3d = system.GPU().Maxwell3D();
    u32 binding = device.GetBaseBindings(stage_index).image;
    for (const auto& entry : shader->GetEntries().images) {
        const auto shader_type = static_cast<Tegra::Engines::ShaderType>(stage_index);
        const auto tic = GetTextureInfo(maxwell3d, entry, shader_type).tic;
        SetupImage(binding++, tic, entry);
    }
}

void RasterizerOpenGL::SetupComputeImages(Shader* shader) {
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
    if (entry.is_written) {
        view->MarkAsModified(texture_cache.Tick());
    }
    const GLuint handle = view->GetTexture(tic.x_source, tic.y_source, tic.z_source, tic.w_source);
    glBindImageTexture(binding, handle, 0, GL_TRUE, 0, GL_READ_WRITE, view->GetFormat());
}

void RasterizerOpenGL::SyncViewport() {
    auto& gpu = system.GPU().Maxwell3D();
    auto& flags = gpu.dirty.flags;
    const auto& regs = gpu.regs;

    const bool dirty_viewport = flags[Dirty::Viewports];
    const bool dirty_clip_control = flags[Dirty::ClipControl];

    if (dirty_clip_control || flags[Dirty::FrontFace]) {
        flags[Dirty::FrontFace] = false;

        GLenum mode = MaxwellToGL::FrontFace(regs.front_face);
        if (regs.screen_y_control.triangle_rast_flip != 0 &&
            regs.viewport_transform[0].scale_y < 0.0f) {
            switch (mode) {
            case GL_CW:
                mode = GL_CCW;
                break;
            case GL_CCW:
                mode = GL_CW;
                break;
            }
        }
        glFrontFace(mode);
    }

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

            if (!GLAD_GL_NV_viewport_swizzle) {
                continue;
            }
            glViewportSwizzleNV(static_cast<GLuint>(i), MaxwellToGL::ViewportSwizzle(src.swizzle.x),
                                MaxwellToGL::ViewportSwizzle(src.swizzle.y),
                                MaxwellToGL::ViewportSwizzle(src.swizzle.z),
                                MaxwellToGL::ViewportSwizzle(src.swizzle.w));
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

    oglEnable(GL_DEPTH_CLAMP, gpu.regs.view_volume_clip_control.depth_clamp_disabled == 0);
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

void RasterizerOpenGL::SyncTransformFeedback() {
    // TODO(Rodrigo): Inject SKIP_COMPONENTS*_NV when required. An unimplemented message will signal
    // when this is required.
    const auto& regs = system.GPU().Maxwell3D().regs;

    static constexpr std::size_t STRIDE = 3;
    std::array<GLint, 128 * STRIDE * Maxwell::NumTransformFeedbackBuffers> attribs;
    std::array<GLint, Maxwell::NumTransformFeedbackBuffers> streams;

    GLint* cursor = attribs.data();
    GLint* current_stream = streams.data();

    for (std::size_t feedback = 0; feedback < Maxwell::NumTransformFeedbackBuffers; ++feedback) {
        const auto& layout = regs.tfb_layouts[feedback];
        UNIMPLEMENTED_IF_MSG(layout.stride != layout.varying_count * 4, "Stride padding");
        if (layout.varying_count == 0) {
            continue;
        }

        *current_stream = static_cast<GLint>(feedback);
        if (current_stream != streams.data()) {
            // When stepping one stream, push the expected token
            cursor[0] = GL_NEXT_BUFFER_NV;
            cursor[1] = 0;
            cursor[2] = 0;
            cursor += STRIDE;
        }
        ++current_stream;

        const auto& locations = regs.tfb_varying_locs[feedback];
        std::optional<u8> current_index;
        for (u32 offset = 0; offset < layout.varying_count; ++offset) {
            const u8 location = locations[offset];
            const u8 index = location / 4;

            if (current_index == index) {
                // Increase number of components of the previous attachment
                ++cursor[-2];
                continue;
            }
            current_index = index;

            std::tie(cursor[0], cursor[2]) = TransformFeedbackEnum(location);
            cursor[1] = 1;
            cursor += STRIDE;
        }
    }

    const GLsizei num_attribs = static_cast<GLsizei>((cursor - attribs.data()) / STRIDE);
    const GLsizei num_strides = static_cast<GLsizei>(current_stream - streams.data());
    glTransformFeedbackStreamAttribsNV(num_attribs, attribs.data(), num_strides, streams.data(),
                                       GL_INTERLEAVED_ATTRIBS);
}

void RasterizerOpenGL::BeginTransformFeedback(GLenum primitive_mode) {
    const auto& regs = system.GPU().Maxwell3D().regs;
    if (regs.tfb_enabled == 0) {
        return;
    }

    if (device.UseAssemblyShaders()) {
        SyncTransformFeedback();
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

    // We may have to call BeginTransformFeedbackNV here since they seem to call different
    // implementations on Nvidia's driver (the pointer is different) but we are using
    // ARB_transform_feedback3 features with NV_transform_feedback interactions and the ARB
    // extension doesn't define BeginTransformFeedback (without NV) interactions. It just works.
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
        const auto info = buffer_cache.UploadMemory(gpu_addr, size, 4, true);
        glCopyNamedBufferSubData(handle, info.handle, 0, info.offset,
                                 static_cast<GLsizeiptr>(size));
    }
}

} // namespace OpenGL
