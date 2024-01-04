// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"

#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_fsr.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"

namespace Core {
class System;
class TelemetrySession;
} // namespace Core

namespace Core::Frontend {
class EmuWindow;
}

namespace Core::Memory {
class Memory;
}

namespace Layout {
struct FramebufferLayout;
}

namespace Tegra {
class GPU;
}

namespace OpenGL {

/// Structure used for storing information about the textures for the Switch screen
struct TextureInfo {
    OGLTexture resource;
    GLsizei width;
    GLsizei height;
    GLenum gl_format;
    GLenum gl_type;
    Service::android::PixelFormat pixel_format;
};

/// Structure used for storing information about the display target for the Switch screen
struct FramebufferTextureInfo {
    GLuint display_texture{};
    u32 width;
    u32 height;
};

class RendererOpenGL final : public VideoCore::RendererBase {
public:
    explicit RendererOpenGL(Core::TelemetrySession& telemetry_session_,
                            Core::Frontend::EmuWindow& emu_window_,
                            Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                            std::unique_ptr<Core::Frontend::GraphicsContext> context_);
    ~RendererOpenGL() override;

    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;

    VideoCore::RasterizerInterface* ReadRasterizer() override {
        return &rasterizer;
    }

    [[nodiscard]] std::string GetDeviceVendor() const override {
        return device.GetVendorName();
    }

private:
    /// Initializes the OpenGL state and creates persistent objects.
    void InitOpenGLObjects();

    void AddTelemetryFields();

    void ConfigureFramebufferTexture(const Tegra::FramebufferConfig& framebuffer);

    /// Draws the emulated screens to the emulator window.
    void DrawScreen(const Tegra::FramebufferConfig& framebuffer,
                    const Layout::FramebufferLayout& layout);

    void RenderScreenshot(const Tegra::FramebufferConfig& framebuffer);

    /// Loads framebuffer from emulated memory into the active OpenGL texture.
    FramebufferTextureInfo LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer);

    FramebufferTextureInfo PrepareRenderTarget(const Tegra::FramebufferConfig& framebuffer);

    Core::TelemetrySession& telemetry_session;
    Core::Frontend::EmuWindow& emu_window;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    Tegra::GPU& gpu;

    Device device;
    StateTracker state_tracker;
    ProgramManager program_manager;
    RasterizerOpenGL rasterizer;

    // OpenGL object IDs
    OGLSampler present_sampler;
    OGLSampler present_sampler_nn;
    OGLBuffer vertex_buffer;
    OGLProgram fxaa_vertex;
    OGLProgram fxaa_fragment;
    OGLProgram present_vertex;
    OGLProgram present_bilinear_fragment;
    OGLProgram present_bicubic_fragment;
    OGLProgram present_gaussian_fragment;
    OGLProgram present_scaleforce_fragment;
    OGLFramebuffer screenshot_framebuffer;

    // GPU address of the vertex buffer
    GLuint64EXT vertex_buffer_address = 0;

    /// Display information for Switch screen
    TextureInfo framebuffer_texture;
    OGLTexture aa_texture;
    OGLFramebuffer aa_framebuffer;

    OGLProgram smaa_edge_detection_vert;
    OGLProgram smaa_blending_weight_calculation_vert;
    OGLProgram smaa_neighborhood_blending_vert;
    OGLProgram smaa_edge_detection_frag;
    OGLProgram smaa_blending_weight_calculation_frag;
    OGLProgram smaa_neighborhood_blending_frag;
    OGLTexture smaa_area_tex;
    OGLTexture smaa_search_tex;
    OGLTexture smaa_edges_tex;
    OGLTexture smaa_blend_tex;

    std::unique_ptr<FSR> fsr;

    /// OpenGL framebuffer data
    std::vector<u8> gl_framebuffer_data;
};

} // namespace OpenGL
