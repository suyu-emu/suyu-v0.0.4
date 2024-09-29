// SPDX-FileCopyrightText: 2015 Citra Emulator Project
// SPDX-FileCopyrightText: 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <array>
#include <bitset>
#include <memory>
#include <string_view>
#include <utility>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "video_core/control/channel_state.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_staging_buffer_pool.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/maxwell_to_gl.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/shader_cache.h"
#include "video_core/texture_cache/texture_cache_base.h"

namespace OpenGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using GLvec4 = std::array<GLfloat, 4>;

using VideoCore::Surface::PixelFormat;
using VideoCore::Surface::SurfaceTarget;
using VideoCore::Surface::SurfaceType;

MICROPROFILE_DEFINE(OpenGL_Drawing, "OpenGL", "Drawing", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Clears, "OpenGL", "Clears", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_Blits, "OpenGL", "Blits", MP_RGB(128, 128, 192));
MICROPROFILE_DEFINE(OpenGL_CacheManagement, "OpenGL", "Cache Management", MP_RGB(100, 255, 100));

namespace {
constexpr size_t NUM_SUPPORTED_VERTEX_ATTRIBUTES = 16;

void oglEnable(GLenum cap, bool state) {
    (state ? glEnable : glDisable)(cap);
}

std::optional<VideoCore::QueryType> MaxwellToVideoCoreQuery(VideoCommon::QueryType type) {
    switch (type) {
    case VideoCommon::QueryType::PrimitivesGenerated:
    case VideoCommon::QueryType::VtgPrimitivesOut:
        return VideoCore::QueryType::PrimitivesGenerated;
    case VideoCommon::QueryType::ZPassPixelCount64:
        return VideoCore::QueryType::SamplesPassed;
    case VideoCommon::QueryType::StreamingPrimitivesSucceeded:
        // case VideoCommon::QueryType::StreamingByteCount:
        // TODO: StreamingByteCount = StreamingPrimitivesSucceeded * num_verts * vert_stride
        return VideoCore::QueryType::TfbPrimitivesWritten;
    default:
        return std::nullopt;
    }
}
} // Anonymous namespace

RasterizerOpenGL::RasterizerOpenGL(Core::Frontend::EmuWindow& emu_window_, Tegra::GPU& gpu_,
                                   Tegra::MaxwellDeviceMemoryManager& device_memory_,
                                   const Device& device_, ProgramManager& program_manager_,
                                   StateTracker& state_tracker_)
    : VideoCommon::OptimizedRasterizer(gpu_, emu_window_), gpu(gpu_),
      device_memory(device_memory_), device(device_), program_manager(program_manager_),
      state_tracker(state_tracker_),
      texture_cache_runtime(device, program_manager, state_tracker, staging_buffer_pool),
      texture_cache(texture_cache_runtime, device_memory_),
      buffer_cache_runtime(device, staging_buffer_pool),
      buffer_cache(device_memory_, buffer_cache_runtime),
      shader_cache(device_memory_, emu_window_, device, texture_cache, buffer_cache,
                   program_manager, state_tracker, gpu.ShaderNotify()),
      query_cache(*this, device_memory_), accelerate_dma(buffer_cache, texture_cache),
      fence_manager(*this, gpu, texture_cache, buffer_cache, query_cache),
      blit_image(program_manager_) {
    // Initialize OpenGL-specific features
    InitializeOpenGLFeatures();
}

RasterizerOpenGL::~RasterizerOpenGL() = default;

void RasterizerOpenGL::InitializeOpenGLFeatures() {
    // Enable additional optimization features
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_PRIMITIVE_RESTART_FIXED_INDEX);
}

void RasterizerOpenGL::Draw(bool is_indexed, u32 instance_count) {
    MICROPROFILE_SCOPE(OpenGL_Drawing);
    SCOPE_EXIT { gpu.TickWork(); };

    PrepareDraw(is_indexed, instance_count);

    const GLenum primitive_mode = MaxwellToGL::PrimitiveTopology(maxwell3d->draw_manager->GetDrawState().topology);
    const GLsizei num_instances = static_cast<GLsizei>(instance_count);

    if (is_indexed) {
        const auto& draw_state = maxwell3d->draw_manager->GetDrawState();
        const GLint base_vertex = static_cast<GLint>(draw_state.base_index);
        const GLsizei num_vertices = static_cast<GLsizei>(draw_state.index_buffer.count);
        const GLvoid* const offset = buffer_cache_runtime.IndexOffset();
        const GLenum format = MaxwellToGL::IndexFormat(draw_state.index_buffer.format);

        if (num_instances == 1 && base_vertex == 0) {
            glDrawElements(primitive_mode, num_vertices, format, offset);
        } else if (num_instances == 1) {
            glDrawElementsBaseVertex(primitive_mode, num_vertices, format, offset, base_vertex);
        } else if (base_vertex == 0) {
            glDrawElementsInstanced(primitive_mode, num_vertices, format, offset, num_instances);
        } else {
            glDrawElementsInstancedBaseVertex(primitive_mode, num_vertices, format, offset,
                                              num_instances, base_vertex);
        }
    } else {
        const auto& draw_state = maxwell3d->draw_manager->GetDrawState();
        const GLint base_vertex = static_cast<GLint>(draw_state.vertex_buffer.first);
        const GLsizei num_vertices = static_cast<GLsizei>(draw_state.vertex_buffer.count);

        if (num_instances == 1) {
            glDrawArrays(primitive_mode, base_vertex, num_vertices);
        } else {
            glDrawArraysInstanced(primitive_mode, base_vertex, num_vertices, num_instances);
        }
    }
}

void RasterizerOpenGL::Clear(u32 layer_count) {
    MICROPROFILE_SCOPE(OpenGL_Clears);

    const auto& regs = maxwell3d->regs;
    bool use_color{};
    bool use_depth{};
    bool use_stencil{};

    if (regs.clear_surface.R || regs.clear_surface.G || regs.clear_surface.B ||
        regs.clear_surface.A) {
        use_color = true;

        const GLuint index = regs.clear_surface.RT;
        state_tracker.NotifyColorMask(index);
        glColorMaski(index, regs.clear_surface.R != 0, regs.clear_surface.G != 0,
                     regs.clear_surface.B != 0, regs.clear_surface.A != 0);

        SyncFragmentColorClampState();
        SyncFramebufferSRGB();
    }
    if (regs.clear_surface.Z) {
        ASSERT_MSG(regs.zeta_enable != 0, "Tried to clear Z but buffer is not enabled!");
        use_depth = true;

        state_tracker.NotifyDepthMask();
        glDepthMask(GL_TRUE);
    }
    if (regs.clear_surface.S) {
        ASSERT_MSG(regs.zeta_enable != 0, "Tried to clear stencil but buffer is not enabled!");
        use_stencil = true;
    }

    if (!use_color && !use_depth && !use_stencil) {
        return;
    }

    SyncRasterizeEnable();
    SyncStencilTestState();

    std::scoped_lock lock{texture_cache.mutex};
    texture_cache.UpdateRenderTargets(true);
    state_tracker.BindFramebuffer(texture_cache.GetFramebuffer()->Handle());
    SyncViewport();

    if (regs.clear_control.use_scissor) {
        SyncScissorTest();
    } else {
        state_tracker.NotifyScissor0();
        glDisablei(GL_SCISSOR_TEST, 0);
    }

    if (use_color) {
        glClearBufferfv(GL_COLOR, regs.clear_surface.RT, regs.clear_color.data());
    }
    if (use_depth && use_stencil) {
        glClearBufferfi(GL_DEPTH_STENCIL, 0, regs.clear_depth, regs.clear_stencil);
    } else if (use_depth) {
        glClearBufferfv(GL_DEPTH, 0, &regs.clear_depth);
    } else if (use_stencil) {
        glClearBufferiv(GL_STENCIL, 0, &regs.clear_stencil);
    }
}

void RasterizerOpenGL::DispatchCompute() {
    MICROPROFILE_SCOPE(OpenGL_Drawing);
    SCOPE_EXIT { gpu.TickWork(); };

    ComputePipeline* const pipeline{shader_cache.CurrentComputePipeline()};
    if (!pipeline) {
        return;
    }

    std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
    pipeline->SetEngine(kepler_compute, gpu_memory);
    pipeline->Configure();

    const auto& qmd{kepler_compute->launch_description};
    const std::array<GLuint, 3> dim{qmd.grid_dim_x, qmd.grid_dim_y, qmd.grid_dim_z};
    glDispatchCompute(dim[0], dim[1], dim[2]);
}

void RasterizerOpenGL::ResetCounter(VideoCommon::QueryType type) {
    const auto query_cache_type = MaxwellToVideoCoreQuery(type);
    if (!query_cache_type.has_value()) {
        UNIMPLEMENTED_IF_MSG(type != VideoCommon::QueryType::Payload, "Unsupported query type: {}", type);
        return;
    }
    query_cache.ResetCounter(*query_cache_type);
}

void RasterizerOpenGL::Query(GPUVAddr gpu_addr, VideoCommon::QueryType type,
                             VideoCommon::QueryPropertiesFlags flags, u32 payload, u32 subreport) {
    const auto query_cache_type = MaxwellToVideoCoreQuery(type);
    if (!query_cache_type.has_value()) {
        return QueryFallback(gpu_addr, type, flags, payload, subreport);
    }
    const bool has_timeout = True(flags & VideoCommon::QueryPropertiesFlags::HasTimeout);
    const auto timestamp = has_timeout ? std::optional<u64>{gpu.GetTicks()} : std::nullopt;
    query_cache.Query(gpu_addr, *query_cache_type, timestamp);
}

void RasterizerOpenGL::BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr,
                                                 u32 size) {
    std::scoped_lock lock{buffer_cache.mutex};
    buffer_cache.BindGraphicsUniformBuffer(stage, index, gpu_addr, size);
}

void RasterizerOpenGL::DisableGraphicsUniformBuffer(size_t stage, u32 index) {
    buffer_cache.DisableGraphicsUniformBuffer(stage, index);
}

void RasterizerOpenGL::FlushAll() {}

void RasterizerOpenGL::FlushRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    if (True(which & VideoCommon::CacheType::TextureCache)) {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.DownloadMemory(addr, size);
    }
    if ((True(which & VideoCommon::CacheType::BufferCache))) {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.DownloadMemory(addr, size);
    }
    if ((True(which & VideoCommon::CacheType::QueryCache))) {
        query_cache.FlushRegion(addr, size);
    }
}

bool RasterizerOpenGL::MustFlushRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    if ((True(which & VideoCommon::CacheType::BufferCache))) {
        std::scoped_lock lock{buffer_cache.mutex};
        if (buffer_cache.IsRegionGpuModified(addr, size)) {
            return true;
        }
    }
    if (!Settings::IsGPULevelHigh()) {
        return false;
    }
    if (True(which & VideoCommon::CacheType::TextureCache)) {
        std::scoped_lock lock{texture_cache.mutex};
        return texture_cache.IsRegionGpuModified(addr, size);
    }
    return false;
}

VideoCore::RasterizerDownloadArea RasterizerOpenGL::GetFlushArea(DAddr addr, u64 size) {
    VideoCore::RasterizerDownloadArea result{
        .start_address = Common::AlignDown(addr, Core::DEVICE_PAGESIZE),
        .end_address = Common::AlignUp(addr + size, Core::DEVICE_PAGESIZE),
        .preemtive = true,
    };
    {
        std::scoped_lock lock{texture_cache.mutex};
        auto area = texture_cache.GetFlushArea(addr, size);
        if (area) {
            result = *area;
            result.preemtive = false;
        }
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        auto area = buffer_cache.GetFlushArea(addr, size);
        if (area) {
            result.start_address = std::min(result.start_address, area->start_address);
            result.end_address = std::max(result.end_address, area->end_address);
            result.preemtive = false;
        }
    }
    return result;
}

void RasterizerOpenGL::InvalidateRegion(DAddr addr, u64 size, VideoCommon::CacheType which) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return;
    }
    if (True(which & VideoCommon::CacheType::TextureCache)) {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    if (True(which & VideoCommon::CacheType::BufferCache)) {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    if (True(which & VideoCommon::CacheType::ShaderCache)) {
        shader_cache.InvalidateRegion(addr, size);
    }
    if (True(which & VideoCommon::CacheType::QueryCache)) {
        query_cache.InvalidateRegion(addr, size);
    }
}

bool RasterizerOpenGL::OnCPUWrite(DAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);
    if (addr == 0 || size == 0) {
        return false;
    }

    {
        std::scoped_lock lock{buffer_cache.mutex};
        if (buffer_cache.OnCPUWrite(addr, size)) {
            return true;
        }
    }

    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }

    shader_cache.InvalidateRegion(addr, size);
    return false;
}

void RasterizerOpenGL::OnCacheInvalidation(DAddr addr, u64 size) {
    MICROPROFILE_SCOPE(OpenGL_CacheManagement);

    if (addr == 0 || size == 0) {
        return;
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.WriteMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    shader_cache.InvalidateRegion(addr, size);
}

void RasterizerOpenGL::InvalidateGPUCache() {
    gpu_memory->FlushCaching();
    shader_cache.SyncGuestHost();
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.FlushCachedWrites();
    }
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.FlushCachedWrites();
    }
}

void RasterizerOpenGL::UnmapMemory(DAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapMemory(addr, size);
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.WriteMemory(addr, size);
    }
    shader_cache.OnCacheInvalidation(addr, size);
}

void RasterizerOpenGL::ModifyGPUMemory(size_t as_id, GPUVAddr addr, u64 size) {
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.UnmapGPUMemory(as_id, addr, size);
    }
}

void RasterizerOpenGL::SignalFence(std::function<void()>&& func) {
    fence_manager.SignalFence(std::move(func));
}

void RasterizerOpenGL::SyncOperation(std::function<void()>&& func) {
    fence_manager.SyncOperation(std::move(func));
}

void RasterizerOpenGL::SignalSyncPoint(u32 value) {
    fence_manager.SignalSyncPoint(value);
}

void RasterizerOpenGL::SignalReference() {
    fence_manager.SignalOrdering();
}

void RasterizerOpenGL::ReleaseFences(bool force) {
    fence_manager.WaitPendingFences(force);
}

void RasterizerOpenGL::FlushAndInvalidateRegion(DAddr addr, u64 size,
                                                VideoCommon::CacheType which) {
    if (Settings::IsGPULevelExtreme()) {
        FlushRegion(addr, size, which);
    }
    InvalidateRegion(addr, size, which);
}

void RasterizerOpenGL::WaitForIdle() {
    glMemoryBarrier(GL_ALL_BARRIER_BITS);
    SignalReference();
}

void RasterizerOpenGL::FragmentBarrier() {
    glTextureBarrier();
    glMemoryBarrier(GL_FRAMEBUFFER_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void RasterizerOpenGL::TiledCacheBarrier() {
    glTextureBarrier();
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

    fence_manager.TickFrame();
    {
        std::scoped_lock lock{texture_cache.mutex};
        texture_cache.TickFrame();
    }
    {
        std::scoped_lock lock{buffer_cache.mutex};
        buffer_cache.TickFrame();
    }
}

bool RasterizerOpenGL::AccelerateConditionalRendering() {
    gpu_memory->FlushCaching();
    if (Settings::IsGPULevelHigh()) {
        return false;
    }
    // Medium / Low Hack: stub any checks on queries written into the buffer cache.
    const GPUVAddr condition_address{maxwell3d->regs.render_enable.Address()};
    Maxwell::ReportSemaphore::Compare cmp;
    if (gpu_memory->IsMemoryDirty(condition_address, sizeof(cmp),
                                  VideoCommon::CacheType::BufferCache)) {
        return true;
    }
    return false;
}

bool RasterizerOpenGL::AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Surface& src,
                                             const Tegra::Engines::Fermi2D::Surface& dst,
                                             const Tegra::Engines::Fermi2D::Config& copy_config) {
    MICROPROFILE_SCOPE(OpenGL_Blits);
    std::scoped_lock lock{texture_cache.mutex};
    return texture_cache.BlitImage(dst, src, copy_config);
}

Tegra::Engines::AccelerateDMAInterface& RasterizerOpenGL::AccessAccelerateDMA() {
    return accelerate_dma;
}

void RasterizerOpenGL::AccelerateInlineToMemory(GPUVAddr address, size_t copy_size,
                                                std::span<const u8> memory) {
    auto cpu_addr = gpu_memory->GpuToCpuAddress(address);
    if (!cpu_addr) [[unlikely]] {
        gpu_memory->WriteBlockUnsafe(address, memory.data(), copy_size);
        return;
    }
    gpu_memory->WriteBlockUnsafe(address, memory.data(), copy_size);
    {
        std::unique_lock<std::recursive_mutex> lock{buffer_cache.mutex};
        if (!buffer_cache.InlineMemory(*cpu_addr, copy_size, memory)) {
            buffer_cache.WriteMemory(*cpu_addr, copy_size);
        }
    }
    {
        std::scoped_lock lock_texture{texture_cache.mutex};
        texture_cache.WriteMemory(*cpu_addr, copy_size);
    }
    shader_cache.InvalidateRegion(*cpu_addr, copy_size);
    query_cache.InvalidateRegion(*cpu_addr, copy_size);
}

void RasterizerOpenGL::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                         const VideoCore::DiskResourceLoadCallback& callback) {
    shader_cache.LoadDiskResources(title_id, stop_loading, callback);
}

void RasterizerOpenGL::InitializeChannel(Tegra::Control::ChannelState& channel) {
    CreateChannel(channel);
    {
        std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
        texture_cache.CreateChannel(channel);
        buffer_cache.CreateChannel(channel);
    }
    shader_cache.CreateChannel(channel);
    query_cache.CreateChannel(channel);
    state_tracker.SetupTables(channel);
}

void RasterizerOpenGL::BindChannel(Tegra::Control::ChannelState& channel) {
    const s32 channel_id = channel.bind_id;
    BindToChannel(channel_id);
    {
        std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
        texture_cache.BindToChannel(channel_id);
        buffer_cache.BindToChannel(channel_id);
    }
    shader_cache.BindToChannel(channel_id);
    query_cache.BindToChannel(channel_id);
    state_tracker.ChangeChannel(channel);
    state_tracker.InvalidateState();
}

void RasterizerOpenGL::ReleaseChannel(s32 channel_id) {
    EraseChannel(channel_id);
    {
        std::scoped_lock lock{buffer_cache.mutex, texture_cache.mutex};
        texture_cache.EraseChannel(channel_id);
        buffer_cache.EraseChannel(channel_id);
    }
    shader_cache.EraseChannel(channel_id);
    query_cache.EraseChannel(channel_id);
}

void RasterizerOpenGL::RegisterTransformFeedback(GPUVAddr tfb_object_addr) {
    buffer_cache_runtime.BindTransformFeedbackObject(tfb_object_addr);
}

} // namespace OpenGL
