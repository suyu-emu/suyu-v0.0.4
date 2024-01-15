// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>

#include "core/hle/service/nvnflinger/pixel_format.h"
#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Tegra {
struct FramebufferConfig;
}

namespace OpenGL {

class Device;
class FSR;
class ProgramManager;
class RasterizerOpenGL;
class SMAA;
class StateTracker;

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
    u32 scaled_width;
    u32 scaled_height;
};

class BlitScreen {
public:
    explicit BlitScreen(RasterizerOpenGL& rasterizer,
                        Tegra::MaxwellDeviceMemoryManager& device_memory,
                        StateTracker& state_tracker, ProgramManager& program_manager,
                        Device& device);
    ~BlitScreen();

    void ConfigureFramebufferTexture(const Tegra::FramebufferConfig& framebuffer);

    /// Draws the emulated screens to the emulator window.
    void DrawScreen(const Tegra::FramebufferConfig& framebuffer,
                    const Layout::FramebufferLayout& layout);

    void RenderScreenshot(const Tegra::FramebufferConfig& framebuffer);

    /// Loads framebuffer from emulated memory into the active OpenGL texture.
    FramebufferTextureInfo LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer);

    FramebufferTextureInfo PrepareRenderTarget(const Tegra::FramebufferConfig& framebuffer);

private:
    RasterizerOpenGL& rasterizer;
    Tegra::MaxwellDeviceMemoryManager& device_memory;
    StateTracker& state_tracker;
    ProgramManager& program_manager;
    Device& device;

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

    /// Display information for Switch screen
    TextureInfo framebuffer_texture;
    OGLTexture aa_texture;
    OGLFramebuffer aa_framebuffer;

    std::unique_ptr<FSR> fsr;
    std::unique_ptr<SMAA> smaa;

    /// OpenGL framebuffer data
    std::vector<u8> gl_framebuffer_data;

    // GPU address of the vertex buffer
    GLuint64EXT vertex_buffer_address = 0;
};

} // namespace OpenGL
