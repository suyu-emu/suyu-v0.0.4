// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <tuple>
#include <utility>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/engines/const_buffer_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/rasterizer_accelerated.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_fence_manager.h"
#include "video_core/renderer_opengl/gl_framebuffer_cache.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_sampler_cache.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/gl_texture_cache.h"
#include "video_core/renderer_opengl/utils.h"
#include "video_core/textures/texture.h"

namespace Core {
class System;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
class MemoryManager;
}

namespace OpenGL {

struct ScreenInfo;
struct DrawParameters;

class RasterizerOpenGL : public VideoCore::RasterizerAccelerated {
public:
    explicit RasterizerOpenGL(Core::System& system, Core::Frontend::EmuWindow& emu_window,
                              ScreenInfo& info, GLShader::ProgramManager& program_manager,
                              StateTracker& state_tracker);
    ~RasterizerOpenGL() override;

    void Draw(bool is_indexed, bool is_instanced) override;
    void Clear() override;
    void DispatchCompute(GPUVAddr code_addr) override;
    void ResetCounter(VideoCore::QueryType type) override;
    void Query(GPUVAddr gpu_addr, VideoCore::QueryType type, std::optional<u64> timestamp) override;
    void FlushAll() override;
    void FlushRegion(VAddr addr, u64 size) override;
    bool MustFlushRegion(VAddr addr, u64 size) override;
    void InvalidateRegion(VAddr addr, u64 size) override;
    void OnCPUWrite(VAddr addr, u64 size) override;
    void SyncGuestHost() override;
    void SignalSemaphore(GPUVAddr addr, u32 value) override;
    void SignalSyncPoint(u32 value) override;
    void ReleaseFences() override;
    void FlushAndInvalidateRegion(VAddr addr, u64 size) override;
    void FlushCommands() override;
    void TickFrame() override;
    bool AccelerateSurfaceCopy(const Tegra::Engines::Fermi2D::Regs::Surface& src,
                               const Tegra::Engines::Fermi2D::Regs::Surface& dst,
                               const Tegra::Engines::Fermi2D::Config& copy_config) override;
    bool AccelerateDisplay(const Tegra::FramebufferConfig& config, VAddr framebuffer_addr,
                           u32 pixel_stride) override;
    void LoadDiskResources(const std::atomic_bool& stop_loading,
                           const VideoCore::DiskResourceLoadCallback& callback) override;
    void SetupDirtyFlags() override;

    /// Returns true when there are commands queued to the OpenGL server.
    bool AnyCommandQueued() const {
        return num_queued_commands > 0;
    }

private:
    /// Configures the color and depth framebuffer states.
    void ConfigureFramebuffers();

    /// Configures the color and depth framebuffer for clearing.
    void ConfigureClearFramebuffer(bool using_color, bool using_depth_stencil);

    /// Configures the current constbuffers to use for the draw command.
    void SetupDrawConstBuffers(std::size_t stage_index, const Shader& shader);

    /// Configures the current constbuffers to use for the kernel invocation.
    void SetupComputeConstBuffers(const Shader& kernel);

    /// Configures a constant buffer.
    void SetupConstBuffer(u32 binding, const Tegra::Engines::ConstBufferInfo& buffer,
                          const ConstBufferEntry& entry);

    /// Configures the current global memory entries to use for the draw command.
    void SetupDrawGlobalMemory(std::size_t stage_index, const Shader& shader);

    /// Configures the current global memory entries to use for the kernel invocation.
    void SetupComputeGlobalMemory(const Shader& kernel);

    /// Configures a constant buffer.
    void SetupGlobalMemory(u32 binding, const GlobalMemoryEntry& entry, GPUVAddr gpu_addr,
                           std::size_t size);

    /// Configures the current textures to use for the draw command.
    void SetupDrawTextures(std::size_t stage_index, const Shader& shader);

    /// Configures the textures used in a compute shader.
    void SetupComputeTextures(const Shader& kernel);

    /// Configures a texture.
    void SetupTexture(u32 binding, const Tegra::Texture::FullTextureInfo& texture,
                      const SamplerEntry& entry);

    /// Configures images in a graphics shader.
    void SetupDrawImages(std::size_t stage_index, const Shader& shader);

    /// Configures images in a compute shader.
    void SetupComputeImages(const Shader& shader);

    /// Configures an image.
    void SetupImage(u32 binding, const Tegra::Texture::TICEntry& tic, const ImageEntry& entry);

    /// Syncs the viewport and depth range to match the guest state
    void SyncViewport();

    /// Syncs the depth clamp state
    void SyncDepthClamp();

    /// Syncs the clip enabled status to match the guest state
    void SyncClipEnabled(u32 clip_mask);

    /// Syncs the clip coefficients to match the guest state
    void SyncClipCoef();

    /// Syncs the cull mode to match the guest state
    void SyncCullMode();

    /// Syncs the primitve restart to match the guest state
    void SyncPrimitiveRestart();

    /// Syncs the depth test state to match the guest state
    void SyncDepthTestState();

    /// Syncs the stencil test state to match the guest state
    void SyncStencilTestState();

    /// Syncs the blend state to match the guest state
    void SyncBlendState();

    /// Syncs the LogicOp state to match the guest state
    void SyncLogicOpState();

    /// Syncs the the color clamp state
    void SyncFragmentColorClampState();

    /// Syncs the alpha coverage and alpha to one
    void SyncMultiSampleState();

    /// Syncs the scissor test state to match the guest state
    void SyncScissorTest();

    /// Syncs the point state to match the guest state
    void SyncPointState();

    /// Syncs the line state to match the guest state
    void SyncLineState();

    /// Syncs the rasterizer enable state to match the guest state
    void SyncRasterizeEnable();

    /// Syncs polygon modes to match the guest state
    void SyncPolygonModes();

    /// Syncs Color Mask
    void SyncColorMask();

    /// Syncs the polygon offsets
    void SyncPolygonOffset();

    /// Syncs the alpha test state to match the guest state
    void SyncAlphaTest();

    /// Syncs the framebuffer sRGB state to match the guest state
    void SyncFramebufferSRGB();

    /// Begin a transform feedback
    void BeginTransformFeedback(GLenum primitive_mode);

    /// End a transform feedback
    void EndTransformFeedback();

    /// Check for extension that are not strictly required but are needed for correct emulation
    void CheckExtensions();

    std::size_t CalculateVertexArraysSize() const;

    std::size_t CalculateIndexBufferSize() const;

    /// Updates the current vertex format
    void SetupVertexFormat();

    void SetupVertexBuffer();
    void SetupVertexInstances();

    GLintptr SetupIndexBuffer();

    void SetupShaders(GLenum primitive_mode);

    const Device device;

    TextureCacheOpenGL texture_cache;
    ShaderCacheOpenGL shader_cache;
    SamplerCacheOpenGL sampler_cache;
    FramebufferCacheOpenGL framebuffer_cache;
    QueryCache query_cache;
    OGLBufferCache buffer_cache;
    FenceManagerOpenGL fence_manager;

    Core::System& system;
    ScreenInfo& screen_info;
    GLShader::ProgramManager& program_manager;
    StateTracker& state_tracker;

    static constexpr std::size_t STREAM_BUFFER_SIZE = 128 * 1024 * 1024;

    GLint vertex_binding = 0;

    std::array<OGLBuffer, Tegra::Engines::Maxwell3D::Regs::NumTransformFeedbackBuffers>
        transform_feedback_buffers;
    std::bitset<Tegra::Engines::Maxwell3D::Regs::NumTransformFeedbackBuffers>
        enabled_transform_feedback_buffers;

    /// Number of commands queued to the OpenGL driver. Reseted on flush.
    std::size_t num_queued_commands = 0;

    u32 last_clip_distance_mask = 0;
};

} // namespace OpenGL
