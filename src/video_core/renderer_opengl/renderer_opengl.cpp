// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "common/telemetry.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/telemetry_session.h"
#include "video_core/renderer_opengl/gl_blit_screen.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/renderer_opengl.h"
#include "video_core/textures/decoders.h"

namespace OpenGL {
namespace {
const char* GetSource(GLenum source) {
    switch (source) {
    case GL_DEBUG_SOURCE_API:
        return "API";
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        return "WINDOW_SYSTEM";
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        return "SHADER_COMPILER";
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        return "THIRD_PARTY";
    case GL_DEBUG_SOURCE_APPLICATION:
        return "APPLICATION";
    case GL_DEBUG_SOURCE_OTHER:
        return "OTHER";
    default:
        ASSERT(false);
        return "Unknown source";
    }
}

const char* GetType(GLenum type) {
    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        return "ERROR";
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        return "DEPRECATED_BEHAVIOR";
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        return "UNDEFINED_BEHAVIOR";
    case GL_DEBUG_TYPE_PORTABILITY:
        return "PORTABILITY";
    case GL_DEBUG_TYPE_PERFORMANCE:
        return "PERFORMANCE";
    case GL_DEBUG_TYPE_OTHER:
        return "OTHER";
    case GL_DEBUG_TYPE_MARKER:
        return "MARKER";
    default:
        ASSERT(false);
        return "Unknown type";
    }
}

void APIENTRY DebugHandler(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length,
                           const GLchar* message, const void* user_param) {
    const char format[] = "{} {} {}: {}";
    const char* const str_source = GetSource(source);
    const char* const str_type = GetType(type);

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        LOG_CRITICAL(Render_OpenGL, format, str_source, str_type, id, message);
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        LOG_WARNING(Render_OpenGL, format, str_source, str_type, id, message);
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
    case GL_DEBUG_SEVERITY_LOW:
        LOG_DEBUG(Render_OpenGL, format, str_source, str_type, id, message);
        break;
    }
}
} // Anonymous namespace

RendererOpenGL::RendererOpenGL(Core::TelemetrySession& telemetry_session_,
                               Core::Frontend::EmuWindow& emu_window_,
                               Tegra::MaxwellDeviceMemoryManager& device_memory_, Tegra::GPU& gpu_,
                               std::unique_ptr<Core::Frontend::GraphicsContext> context_)
    : RendererBase{emu_window_, std::move(context_)}, telemetry_session{telemetry_session_},
      emu_window{emu_window_}, device_memory{device_memory_}, gpu{gpu_}, device{emu_window_},
      state_tracker{}, program_manager{device},
      rasterizer(emu_window, gpu, device_memory, device, program_manager, state_tracker) {
    if (Settings::values.renderer_debug && GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(DebugHandler, nullptr);
    }
    AddTelemetryFields();

    // Initialize default attributes to match hardware's disabled attributes
    GLint max_attribs{};
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &max_attribs);
    for (GLint attrib = 0; attrib < max_attribs; ++attrib) {
        glVertexAttrib4f(attrib, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    // Enable seamless cubemaps when per texture parameters are not available
    if (!GLAD_GL_ARB_seamless_cubemap_per_texture && !GLAD_GL_AMD_seamless_cubemap_per_texture) {
        glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    }
    blit_screen = std::make_unique<BlitScreen>(rasterizer, device_memory, state_tracker,
                                               program_manager, device);
}

RendererOpenGL::~RendererOpenGL() = default;

void RendererOpenGL::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    if (!framebuffer) {
        return;
    }

    RenderScreenshot(*framebuffer);

    state_tracker.BindFramebuffer(0);
    blit_screen->DrawScreen(*framebuffer, emu_window.GetFramebufferLayout());

    ++m_current_frame;

    gpu.RendererFrameEndNotify();
    rasterizer.TickFrame();

    context->SwapBuffers();
    render_window.OnFrameDisplayed();
}

void RendererOpenGL::AddTelemetryFields() {
    const char* const gl_version{reinterpret_cast<char const*>(glGetString(GL_VERSION))};
    const char* const gpu_vendor{reinterpret_cast<char const*>(glGetString(GL_VENDOR))};
    const char* const gpu_model{reinterpret_cast<char const*>(glGetString(GL_RENDERER))};

    LOG_INFO(Render_OpenGL, "GL_VERSION: {}", gl_version);
    LOG_INFO(Render_OpenGL, "GL_VENDOR: {}", gpu_vendor);
    LOG_INFO(Render_OpenGL, "GL_RENDERER: {}", gpu_model);

    constexpr auto user_system = Common::Telemetry::FieldType::UserSystem;
    telemetry_session.AddField(user_system, "GPU_Vendor", std::string(gpu_vendor));
    telemetry_session.AddField(user_system, "GPU_Model", std::string(gpu_model));
    telemetry_session.AddField(user_system, "GPU_OpenGL_Version", std::string(gl_version));
}

void RendererOpenGL::RenderScreenshot(const Tegra::FramebufferConfig& framebuffer) {
    if (!renderer_settings.screenshot_requested) {
        return;
    }

    GLint old_read_fb;
    GLint old_draw_fb;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_fb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw_fb);

    // Draw the current frame to the screenshot framebuffer
    screenshot_framebuffer.Create();
    glBindFramebuffer(GL_FRAMEBUFFER, screenshot_framebuffer.handle);

    const Layout::FramebufferLayout layout{renderer_settings.screenshot_framebuffer_layout};

    GLuint renderbuffer;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_SRGB8, layout.width, layout.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

    blit_screen->DrawScreen(framebuffer, layout);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    glReadPixels(0, 0, layout.width, layout.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                 renderer_settings.screenshot_bits);

    screenshot_framebuffer.Release();
    glDeleteRenderbuffers(1, &renderbuffer);

    glBindFramebuffer(GL_READ_FRAMEBUFFER, old_read_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw_fb);

    renderer_settings.screenshot_complete_callback(true);
    renderer_settings.screenshot_requested = false;
}

} // namespace OpenGL
