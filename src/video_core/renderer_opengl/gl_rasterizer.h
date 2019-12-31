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
    void FlushRegion(CacheAddr addr, u64 size) override;
    void InvalidateRegion(CacheAddr addr, u64 size) override;
    void FlushAndInvalidateRegion(CacheAddr addr, u64 size) override;
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

    void ConfigureClearFramebuffer(bool using_color_fb, bool using_depth_fb, bool using_stencil_fb);

    /// Configures the current constbuffers to use for the draw command.
    void SetupDrawConstBuffers(std::size_t stage_index, const Shader& shader);

    /// Configures the current constbuffers to use for the kernel invocation.
    void SetupComputeConstBuffers(const Shader& kernel);

    /// Configures a constant buffer.
    void SetupConstBuffer(u32 binding, const Tegra::Engines::ConstBufferInfo& buffer,
                          const GLShader::ConstBufferEntry& entry);

    /// Configures the current global memory entries to use for the draw command.
    void SetupDrawGlobalMemory(std::size_t stage_index, const Shader& shader);

    /// Configures the current global memory entries to use for the kernel invocation.
    void SetupComputeGlobalMemory(const Shader& kernel);

    /// Configures a constant buffer.
    void SetupGlobalMemory(u32 binding, const GLShader::GlobalMemoryEntry& entry, GPUVAddr gpu_addr,
                           std::size_t size);

    /// Configures the current textures to use for the draw command.
    void SetupDrawTextures(std::size_t stage_index, const Shader& shader);

    /// Configures the textures used in a compute shader.
    void SetupComputeTextures(const Shader& kernel);

    /// Configures a texture.
    void SetupTexture(u32 binding, const Tegra::Texture::FullTextureInfo& texture,
                      const GLShader::SamplerEntry& entry);

    /// Configures images in a graphics shader.
    void SetupDrawImages(std::size_t stage_index, const Shader& shader);

    /// Configures images in a compute shader.
    void SetupComputeImages(const Shader& shader);

    /// Configures an image.
    void SetupImage(u32 binding, const Tegra::Texture::TICEntry& tic,
                    const GLShader::ImageEntry& entry);

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

    /// Syncs the transform feedback state to match the guest state
    void SyncTransformFeedback();

    /// Syncs the point state to match the guest state
    void SyncPointState();

    /// Syncs the rasterizer enable state to match the guest state
    void SyncRasterizeEnable();

    /// Syncs Color Mask
    void SyncColorMask();

    /// Syncs the polygon offsets
    void SyncPolygonOffset();

    /// Syncs the alpha test state to match the guest state
    void SyncAlphaTest();

    /// Syncs the framebuffer sRGB state to match the guest state
    void SyncFramebufferSRGB();

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

    Core::System& system;
    ScreenInfo& screen_info;
    GLShader::ProgramManager& program_manager;
    StateTracker& state_tracker;

    static constexpr std::size_t STREAM_BUFFER_SIZE = 128 * 1024 * 1024;
    OGLBufferCache buffer_cache;

    VertexArrayPushBuffer vertex_array_pushbuffer{state_tracker};
    BindBuffersRangePushBuffer bind_ubo_pushbuffer{GL_UNIFORM_BUFFER};
    BindBuffersRangePushBuffer bind_ssbo_pushbuffer{GL_SHADER_STORAGE_BUFFER};

    /// Number of commands queued to the OpenGL driver. Reseted on flush.
    std::size_t num_queued_commands = 0;

    u32 last_clip_distance_mask = 0;
};

} // namespace OpenGL
