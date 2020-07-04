// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"

namespace Core {
class System;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Layout {
struct FramebufferLayout;
}

namespace OpenGL {

/// Structure used for storing information about the textures for the Switch screen
struct TextureInfo {
    OGLTexture resource;
    GLsizei width;
    GLsizei height;
    GLenum gl_format;
    GLenum gl_type;
    Tegra::FramebufferConfig::PixelFormat pixel_format;
};

/// Structure used for storing information about the display target for the Switch screen
struct ScreenInfo {
    GLuint display_texture{};
    bool display_srgb{};
    const Common::Rectangle<float> display_texcoords{0.0f, 0.0f, 1.0f, 1.0f};
    TextureInfo texture;
};

struct PresentationTexture {
    u32 width = 0;
    u32 height = 0;
    OGLTexture texture;
};

class FrameMailbox;

class RendererOpenGL final : public VideoCore::RendererBase {
public:
    explicit RendererOpenGL(Core::Frontend::EmuWindow& emu_window, Core::System& system,
                            Core::Frontend::GraphicsContext& context);
    ~RendererOpenGL() override;

    bool Init() override;
    void ShutDown() override;
    void SwapBuffers(const Tegra::FramebufferConfig* framebuffer) override;
    bool TryPresent(int timeout_ms) override;

private:
    /// Initializes the OpenGL state and creates persistent objects.
    void InitOpenGLObjects();

    void AddTelemetryFields();

    void CreateRasterizer();

    void ConfigureFramebufferTexture(TextureInfo& texture,
                                     const Tegra::FramebufferConfig& framebuffer);

    /// Draws the emulated screens to the emulator window.
    void DrawScreen(const Layout::FramebufferLayout& layout);

    void RenderScreenshot();

    /// Loads framebuffer from emulated memory into the active OpenGL texture.
    void LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer);

    /// Fills active OpenGL texture with the given RGB color.Since the color is solid, the texture
    /// can be 1x1 but will stretch across whatever it's rendered on.
    void LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, u8 color_a,
                                    const TextureInfo& texture);

    void PrepareRendertarget(const Tegra::FramebufferConfig* framebuffer);

    bool Present(int timeout_ms);

    Core::Frontend::EmuWindow& emu_window;
    Core::System& system;
    Core::Frontend::GraphicsContext& context;
    const Device device;

    StateTracker state_tracker{system};

    // OpenGL object IDs
    OGLBuffer vertex_buffer;
    OGLProgram vertex_program;
    OGLProgram fragment_program;
    OGLPipeline pipeline;
    OGLFramebuffer screenshot_framebuffer;

    // GPU address of the vertex buffer
    GLuint64EXT vertex_buffer_address = 0;

    /// Display information for Switch screen
    ScreenInfo screen_info;

    /// Global dummy shader pipeline
    ProgramManager program_manager;

    /// OpenGL framebuffer data
    std::vector<u8> gl_framebuffer_data;

    /// Used for transforming the framebuffer orientation
    Tegra::FramebufferConfig::TransformFlags framebuffer_transform_flags;
    Common::Rectangle<int> framebuffer_crop_rect;

    /// Frame presentation mailbox
    std::unique_ptr<FrameMailbox> frame_mailbox;

    bool has_debug_tool = false;
};

} // namespace OpenGL
