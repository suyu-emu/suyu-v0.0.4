// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/telemetry.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"
#include "core/perf_stats.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "video_core/morton.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/renderer_opengl.h"

namespace OpenGL {

// If the size of this is too small, it ends up creating a soft cap on FPS as the renderer will have
// to wait on available presentation frames.
constexpr std::size_t SWAP_CHAIN_SIZE = 3;

struct Frame {
    u32 width{};                      /// Width of the frame (to detect resize)
    u32 height{};                     /// Height of the frame
    bool color_reloaded{};            /// Texture attachment was recreated (ie: resized)
    OpenGL::OGLRenderbuffer color{};  /// Buffer shared between the render/present FBO
    OpenGL::OGLFramebuffer render{};  /// FBO created on the render thread
    OpenGL::OGLFramebuffer present{}; /// FBO created on the present thread
    GLsync render_fence{};            /// Fence created on the render thread
    GLsync present_fence{};           /// Fence created on the presentation thread
    bool is_srgb{};                   /// Framebuffer is sRGB or RGB
};

/**
 * For smooth Vsync rendering, we want to always present the latest frame that the core generates,
 * but also make sure that rendering happens at the pace that the frontend dictates. This is a
 * helper class that the renderer uses to sync frames between the render thread and the presentation
 * thread
 */
class FrameMailbox {
public:
    std::mutex swap_chain_lock;
    std::condition_variable present_cv;
    std::array<Frame, SWAP_CHAIN_SIZE> swap_chain{};
    std::queue<Frame*> free_queue;
    std::deque<Frame*> present_queue;
    Frame* previous_frame{};

    FrameMailbox() {
        for (auto& frame : swap_chain) {
            free_queue.push(&frame);
        }
    }

    ~FrameMailbox() {
        // lock the mutex and clear out the present and free_queues and notify any people who are
        // blocked to prevent deadlock on shutdown
        std::scoped_lock lock{swap_chain_lock};
        std::queue<Frame*>().swap(free_queue);
        present_queue.clear();
        present_cv.notify_all();
    }

    void ReloadPresentFrame(Frame* frame, u32 height, u32 width) {
        frame->present.Release();
        frame->present.Create();
        GLint previous_draw_fbo{};
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previous_draw_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, frame->present.handle);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  frame->color.handle);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_CRITICAL(Render_OpenGL, "Failed to recreate present FBO!");
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, previous_draw_fbo);
        frame->color_reloaded = false;
    }

    void ReloadRenderFrame(Frame* frame, u32 width, u32 height) {
        OpenGLState prev_state = OpenGLState::GetCurState();
        OpenGLState state = OpenGLState::GetCurState();

        // Recreate the color texture attachment
        frame->color.Release();
        frame->color.Create();
        state.renderbuffer = frame->color.handle;
        state.Apply();
        glRenderbufferStorage(GL_RENDERBUFFER, frame->is_srgb ? GL_SRGB8 : GL_RGB8, width, height);

        // Recreate the FBO for the render target
        frame->render.Release();
        frame->render.Create();
        state.draw.read_framebuffer = frame->render.handle;
        state.draw.draw_framebuffer = frame->render.handle;
        state.Apply();
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER,
                                  frame->color.handle);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            LOG_CRITICAL(Render_OpenGL, "Failed to recreate render FBO!");
        }
        prev_state.Apply();
        frame->width = width;
        frame->height = height;
        frame->color_reloaded = true;
    }

    Frame* GetRenderFrame() {
        std::unique_lock lock{swap_chain_lock};

        // If theres no free frames, we will reuse the oldest render frame
        if (free_queue.empty()) {
            auto frame = present_queue.back();
            present_queue.pop_back();
            return frame;
        }

        Frame* frame = free_queue.front();
        free_queue.pop();
        return frame;
    }

    void ReleaseRenderFrame(Frame* frame) {
        std::unique_lock lock{swap_chain_lock};
        present_queue.push_front(frame);
        present_cv.notify_one();
    }

    Frame* TryGetPresentFrame(int timeout_ms) {
        std::unique_lock lock{swap_chain_lock};
        // wait for new entries in the present_queue
        present_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                            [&] { return !present_queue.empty(); });
        if (present_queue.empty()) {
            // timed out waiting for a frame to draw so return the previous frame
            return previous_frame;
        }

        // free the previous frame and add it back to the free queue
        if (previous_frame) {
            free_queue.push(previous_frame);
        }

        // the newest entries are pushed to the front of the queue
        Frame* frame = present_queue.front();
        present_queue.pop_front();
        // remove all old entries from the present queue and move them back to the free_queue
        for (auto f : present_queue) {
            free_queue.push(f);
        }
        present_queue.clear();
        previous_frame = frame;
        return frame;
    }
};

namespace {

constexpr char vertex_shader[] = R"(
#version 430 core

layout (location = 0) in vec2 vert_position;
layout (location = 1) in vec2 vert_tex_coord;
layout (location = 0) out vec2 frag_tex_coord;

// This is a truncated 3x3 matrix for 2D transformations:
// The upper-left 2x2 submatrix performs scaling/rotation/mirroring.
// The third column performs translation.
// The third row could be used for projection, which we don't need in 2D. It hence is assumed to
// implicitly be [0, 0, 1]
layout (location = 0) uniform mat3x2 modelview_matrix;

void main() {
    // Multiply input position by the rotscale part of the matrix and then manually translate by
    // the last column. This is equivalent to using a full 3x3 matrix and expanding the vector
    // to `vec3(vert_position.xy, 1.0)`
    gl_Position = vec4(mat2(modelview_matrix) * vert_position + modelview_matrix[2], 0.0, 1.0);
    frag_tex_coord = vert_tex_coord;
}
)";

constexpr char fragment_shader[] = R"(
#version 430 core

layout (location = 0) in vec2 frag_tex_coord;
layout (location = 0) out vec4 color;

layout (binding = 0) uniform sampler2D color_texture;

void main() {
    color = texture(color_texture, frag_tex_coord);
}
)";

constexpr GLint PositionLocation = 0;
constexpr GLint TexCoordLocation = 1;
constexpr GLint ModelViewMatrixLocation = 0;

struct ScreenRectVertex {
    constexpr ScreenRectVertex(GLfloat x, GLfloat y, GLfloat u, GLfloat v)
        : position{{x, y}}, tex_coord{{u, v}} {}

    std::array<GLfloat, 2> position;
    std::array<GLfloat, 2> tex_coord;
};

/**
 * Defines a 1:1 pixel ortographic projection matrix with (0,0) on the top-left
 * corner and (width, height) on the lower-bottom.
 *
 * The projection part of the matrix is trivial, hence these operations are represented
 * by a 3x2 matrix.
 */
std::array<GLfloat, 3 * 2> MakeOrthographicMatrix(float width, float height) {
    std::array<GLfloat, 3 * 2> matrix; // Laid out in column-major order

    // clang-format off
    matrix[0] = 2.f / width; matrix[2] =  0.f;          matrix[4] = -1.f;
    matrix[1] = 0.f;         matrix[3] = -2.f / height; matrix[5] =  1.f;
    // Last matrix row is implicitly assumed to be [0, 0, 1].
    // clang-format on

    return matrix;
}

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
        UNREACHABLE();
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
        UNREACHABLE();
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

RendererOpenGL::RendererOpenGL(Core::Frontend::EmuWindow& emu_window, Core::System& system)
    : VideoCore::RendererBase{emu_window}, emu_window{emu_window}, system{system},
      frame_mailbox{std::make_unique<FrameMailbox>()} {}

RendererOpenGL::~RendererOpenGL() = default;

MICROPROFILE_DEFINE(OpenGL_RenderFrame, "OpenGL", "Render Frame", MP_RGB(128, 128, 64));
MICROPROFILE_DEFINE(OpenGL_WaitPresent, "OpenGL", "Wait For Present", MP_RGB(128, 128, 128));

void RendererOpenGL::SwapBuffers(const Tegra::FramebufferConfig* framebuffer) {
    render_window.PollEvents();

    if (!framebuffer) {
        return;
    }

    PrepareRendertarget(framebuffer);
    RenderScreenshot();

    Frame* frame;
    {
        MICROPROFILE_SCOPE(OpenGL_WaitPresent);

        frame = frame_mailbox->GetRenderFrame();

        // Clean up sync objects before drawing

        // INTEL driver workaround. We can't delete the previous render sync object until we are
        // sure that the presentation is done
        if (frame->present_fence) {
            glClientWaitSync(frame->present_fence, 0, GL_TIMEOUT_IGNORED);
        }

        // delete the draw fence if the frame wasn't presented
        if (frame->render_fence) {
            glDeleteSync(frame->render_fence);
            frame->render_fence = 0;
        }

        // wait for the presentation to be done
        if (frame->present_fence) {
            glWaitSync(frame->present_fence, 0, GL_TIMEOUT_IGNORED);
            glDeleteSync(frame->present_fence);
            frame->present_fence = 0;
        }
    }

    {
        MICROPROFILE_SCOPE(OpenGL_RenderFrame);
        const auto& layout = render_window.GetFramebufferLayout();

        // Recreate the frame if the size of the window has changed
        if (layout.width != frame->width || layout.height != frame->height ||
            screen_info.display_srgb != frame->is_srgb) {
            LOG_DEBUG(Render_OpenGL, "Reloading render frame");
            frame->is_srgb = screen_info.display_srgb;
            frame_mailbox->ReloadRenderFrame(frame, layout.width, layout.height);
        }
        state.draw.draw_framebuffer = frame->render.handle;
        state.Apply();
        DrawScreen(layout);
        // Create a fence for the frontend to wait on and swap this frame to OffTex
        frame->render_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        glFlush();
        frame_mailbox->ReleaseRenderFrame(frame);
        m_current_frame++;
        rasterizer->TickFrame();
    }
}

void RendererOpenGL::PrepareRendertarget(const Tegra::FramebufferConfig* framebuffer) {
    if (framebuffer) {
        // If framebuffer is provided, reload it from memory to a texture
        if (screen_info.texture.width != static_cast<GLsizei>(framebuffer->width) ||
            screen_info.texture.height != static_cast<GLsizei>(framebuffer->height) ||
            screen_info.texture.pixel_format != framebuffer->pixel_format ||
            gl_framebuffer_data.empty()) {
            // Reallocate texture if the framebuffer size has changed.
            // This is expected to not happen very often and hence should not be a
            // performance problem.
            ConfigureFramebufferTexture(screen_info.texture, *framebuffer);
        }

        // Load the framebuffer from memory, draw it to the screen, and swap buffers
        LoadFBToScreenInfo(*framebuffer);
    }
}

void RendererOpenGL::LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer) {
    // Framebuffer orientation handling
    framebuffer_transform_flags = framebuffer.transform_flags;
    framebuffer_crop_rect = framebuffer.crop_rect;

    const VAddr framebuffer_addr{framebuffer.address + framebuffer.offset};
    if (rasterizer->AccelerateDisplay(framebuffer, framebuffer_addr, framebuffer.stride)) {
        return;
    }

    // Reset the screen info's display texture to its own permanent texture
    screen_info.display_texture = screen_info.texture.resource.handle;

    const auto pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format)};
    const u32 bytes_per_pixel{VideoCore::Surface::GetBytesPerPixel(pixel_format)};
    const u64 size_in_bytes{framebuffer.stride * framebuffer.height * bytes_per_pixel};
    u8* const host_ptr{system.Memory().GetPointer(framebuffer_addr)};
    rasterizer->FlushRegion(ToCacheAddr(host_ptr), size_in_bytes);

    // TODO(Rodrigo): Read this from HLE
    constexpr u32 block_height_log2 = 4;
    VideoCore::MortonSwizzle(VideoCore::MortonSwizzleMode::MortonToLinear, pixel_format,
                             framebuffer.stride, block_height_log2, framebuffer.height, 0, 1, 1,
                             gl_framebuffer_data.data(), host_ptr);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(framebuffer.stride));

    // Update existing texture
    // TODO: Test what happens on hardware when you change the framebuffer dimensions so that
    //       they differ from the LCD resolution.
    // TODO: Applications could theoretically crash yuzu here by specifying too large
    //       framebuffer sizes. We should make sure that this cannot happen.
    glTextureSubImage2D(screen_info.texture.resource.handle, 0, 0, 0, framebuffer.width,
                        framebuffer.height, screen_info.texture.gl_format,
                        screen_info.texture.gl_type, gl_framebuffer_data.data());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void RendererOpenGL::LoadColorToActiveGLTexture(u8 color_r, u8 color_g, u8 color_b, u8 color_a,
                                                const TextureInfo& texture) {
    const u8 framebuffer_data[4] = {color_a, color_b, color_g, color_r};
    glClearTexImage(texture.resource.handle, 0, GL_RGBA, GL_UNSIGNED_BYTE, framebuffer_data);
}

void RendererOpenGL::InitOpenGLObjects() {
    glClearColor(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue,
                 0.0f);

    // Link shaders and get variable locations
    shader.CreateFromSource(vertex_shader, nullptr, fragment_shader);
    state.draw.shader_program = shader.handle;
    state.Apply();

    // Generate VBO handle for drawing
    vertex_buffer.Create();

    // Generate VAO
    vertex_array.Create();
    state.draw.vertex_array = vertex_array.handle;

    // Attach vertex data to VAO
    glNamedBufferData(vertex_buffer.handle, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);
    glVertexArrayAttribFormat(vertex_array.handle, PositionLocation, 2, GL_FLOAT, GL_FALSE,
                              offsetof(ScreenRectVertex, position));
    glVertexArrayAttribFormat(vertex_array.handle, TexCoordLocation, 2, GL_FLOAT, GL_FALSE,
                              offsetof(ScreenRectVertex, tex_coord));
    glVertexArrayAttribBinding(vertex_array.handle, PositionLocation, 0);
    glVertexArrayAttribBinding(vertex_array.handle, TexCoordLocation, 0);
    glEnableVertexArrayAttrib(vertex_array.handle, PositionLocation);
    glEnableVertexArrayAttrib(vertex_array.handle, TexCoordLocation);
    glVertexArrayVertexBuffer(vertex_array.handle, 0, vertex_buffer.handle, 0,
                              sizeof(ScreenRectVertex));

    // Allocate textures for the screen
    screen_info.texture.resource.Create(GL_TEXTURE_2D);

    const GLuint texture = screen_info.texture.resource.handle;
    glTextureStorage2D(texture, 1, GL_RGBA8, 1, 1);

    screen_info.display_texture = screen_info.texture.resource.handle;

    // Clear screen to black
    LoadColorToActiveGLTexture(0, 0, 0, 0, screen_info.texture);
}

void RendererOpenGL::AddTelemetryFields() {
    const char* const gl_version{reinterpret_cast<char const*>(glGetString(GL_VERSION))};
    const char* const gpu_vendor{reinterpret_cast<char const*>(glGetString(GL_VENDOR))};
    const char* const gpu_model{reinterpret_cast<char const*>(glGetString(GL_RENDERER))};

    LOG_INFO(Render_OpenGL, "GL_VERSION: {}", gl_version);
    LOG_INFO(Render_OpenGL, "GL_VENDOR: {}", gpu_vendor);
    LOG_INFO(Render_OpenGL, "GL_RENDERER: {}", gpu_model);

    auto& telemetry_session = system.TelemetrySession();
    telemetry_session.AddField(Telemetry::FieldType::UserSystem, "GPU_Vendor", gpu_vendor);
    telemetry_session.AddField(Telemetry::FieldType::UserSystem, "GPU_Model", gpu_model);
    telemetry_session.AddField(Telemetry::FieldType::UserSystem, "GPU_OpenGL_Version", gl_version);
}

void RendererOpenGL::CreateRasterizer() {
    if (rasterizer) {
        return;
    }
    rasterizer = std::make_unique<RasterizerOpenGL>(system, emu_window, screen_info);
}

void RendererOpenGL::ConfigureFramebufferTexture(TextureInfo& texture,
                                                 const Tegra::FramebufferConfig& framebuffer) {
    texture.width = framebuffer.width;
    texture.height = framebuffer.height;
    texture.pixel_format = framebuffer.pixel_format;

    const auto pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format)};
    const u32 bytes_per_pixel{VideoCore::Surface::GetBytesPerPixel(pixel_format)};
    gl_framebuffer_data.resize(texture.width * texture.height * bytes_per_pixel);

    GLint internal_format;
    switch (framebuffer.pixel_format) {
    case Tegra::FramebufferConfig::PixelFormat::ABGR8:
        internal_format = GL_RGBA8;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        break;
    case Tegra::FramebufferConfig::PixelFormat::RGB565:
        internal_format = GL_RGB565;
        texture.gl_format = GL_RGB;
        texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;
    default:
        internal_format = GL_RGBA8;
        texture.gl_format = GL_RGBA;
        texture.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        UNIMPLEMENTED_MSG("Unknown framebuffer pixel format: {}",
                          static_cast<u32>(framebuffer.pixel_format));
    }

    texture.resource.Release();
    texture.resource.Create(GL_TEXTURE_2D);
    glTextureStorage2D(texture.resource.handle, 1, internal_format, texture.width, texture.height);
}

void RendererOpenGL::DrawScreenTriangles(const ScreenInfo& screen_info, float x, float y, float w,
                                         float h) {
    const auto& texcoords = screen_info.display_texcoords;
    auto left = texcoords.left;
    auto right = texcoords.right;
    if (framebuffer_transform_flags != Tegra::FramebufferConfig::TransformFlags::Unset) {
        if (framebuffer_transform_flags == Tegra::FramebufferConfig::TransformFlags::FlipV) {
            // Flip the framebuffer vertically
            left = texcoords.right;
            right = texcoords.left;
        } else {
            // Other transformations are unsupported
            LOG_CRITICAL(Render_OpenGL, "Unsupported framebuffer_transform_flags={}",
                         static_cast<u32>(framebuffer_transform_flags));
            UNIMPLEMENTED();
        }
    }

    ASSERT_MSG(framebuffer_crop_rect.top == 0, "Unimplemented");
    ASSERT_MSG(framebuffer_crop_rect.left == 0, "Unimplemented");

    // Scale the output by the crop width/height. This is commonly used with 1280x720 rendering
    // (e.g. handheld mode) on a 1920x1080 framebuffer.
    f32 scale_u = 1.f, scale_v = 1.f;
    if (framebuffer_crop_rect.GetWidth() > 0) {
        scale_u = static_cast<f32>(framebuffer_crop_rect.GetWidth()) /
                  static_cast<f32>(screen_info.texture.width);
    }
    if (framebuffer_crop_rect.GetHeight() > 0) {
        scale_v = static_cast<f32>(framebuffer_crop_rect.GetHeight()) /
                  static_cast<f32>(screen_info.texture.height);
    }

    const std::array vertices = {
        ScreenRectVertex(x, y, texcoords.top * scale_u, left * scale_v),
        ScreenRectVertex(x + w, y, texcoords.bottom * scale_u, left * scale_v),
        ScreenRectVertex(x, y + h, texcoords.top * scale_u, right * scale_v),
        ScreenRectVertex(x + w, y + h, texcoords.bottom * scale_u, right * scale_v),
    };

    state.textures[0] = screen_info.display_texture;
    state.framebuffer_srgb.enabled = screen_info.display_srgb;
    state.Apply();

    // TODO: Signal state tracker about these changes
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    glNamedBufferSubData(vertex_buffer.handle, 0, sizeof(vertices), std::data(vertices));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    // Restore default state
    state.framebuffer_srgb.enabled = false;
    state.textures[0] = 0;
    state.Apply();
}

void RendererOpenGL::DrawScreen(const Layout::FramebufferLayout& layout) {
    if (renderer_settings.set_background_color) {
        // Update background color before drawing
        glClearColor(Settings::values.bg_red, Settings::values.bg_green, Settings::values.bg_blue,
                     0.0f);
    }

    const auto& screen = layout.screen;

    glViewport(0, 0, layout.width, layout.height);
    glClear(GL_COLOR_BUFFER_BIT);

    // Set projection matrix
    const std::array ortho_matrix =
        MakeOrthographicMatrix(static_cast<float>(layout.width), static_cast<float>(layout.height));
    glUniformMatrix3x2fv(ModelViewMatrixLocation, 1, GL_FALSE, ortho_matrix.data());

    DrawScreenTriangles(screen_info, static_cast<float>(screen.left),
                        static_cast<float>(screen.top), static_cast<float>(screen.GetWidth()),
                        static_cast<float>(screen.GetHeight()));
}

void RendererOpenGL::TryPresent(int timeout_ms) {
    const auto& layout = render_window.GetFramebufferLayout();
    auto frame = frame_mailbox->TryGetPresentFrame(timeout_ms);
    if (!frame) {
        LOG_DEBUG(Render_OpenGL, "TryGetPresentFrame returned no frame to present");
        return;
    }

    // Clearing before a full overwrite of a fbo can signal to drivers that they can avoid a
    // readback since we won't be doing any blending
    glClear(GL_COLOR_BUFFER_BIT);

    // Recreate the presentation FBO if the color attachment was changed
    if (frame->color_reloaded) {
        LOG_DEBUG(Render_OpenGL, "Reloading present frame");
        frame_mailbox->ReloadPresentFrame(frame, layout.width, layout.height);
    }
    glWaitSync(frame->render_fence, 0, GL_TIMEOUT_IGNORED);
    // INTEL workaround.
    // Normally we could just delete the draw fence here, but due to driver bugs, we can just delete
    // it on the emulation thread without too much penalty
    // glDeleteSync(frame.render_sync);
    // frame.render_sync = 0;

    glBindFramebuffer(GL_READ_FRAMEBUFFER, frame->present.handle);
    glBlitFramebuffer(0, 0, frame->width, frame->height, 0, 0, layout.width, layout.height,
                      GL_COLOR_BUFFER_BIT, GL_LINEAR);

    // Insert fence for the main thread to block on
    frame->present_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    glFlush();

    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
}

void RendererOpenGL::RenderScreenshot() {
    if (!renderer_settings.screenshot_requested) {
        return;
    }

    // Draw the current frame to the screenshot framebuffer
    screenshot_framebuffer.Create();
    GLuint old_read_fb = state.draw.read_framebuffer;
    GLuint old_draw_fb = state.draw.draw_framebuffer;
    state.draw.read_framebuffer = state.draw.draw_framebuffer = screenshot_framebuffer.handle;
    state.Apply();

    Layout::FramebufferLayout layout{renderer_settings.screenshot_framebuffer_layout};

    GLuint renderbuffer;
    glGenRenderbuffers(1, &renderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, screen_info.display_srgb ? GL_SRGB8 : GL_RGB8,
                          layout.width, layout.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, renderbuffer);

    DrawScreen(layout);

    glReadPixels(0, 0, layout.width, layout.height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV,
                 renderer_settings.screenshot_bits);

    screenshot_framebuffer.Release();
    state.draw.read_framebuffer = old_read_fb;
    state.draw.draw_framebuffer = old_draw_fb;
    state.Apply();
    glDeleteRenderbuffers(1, &renderbuffer);

    renderer_settings.screenshot_complete_callback();
    renderer_settings.screenshot_requested = false;
}

bool RendererOpenGL::Init() {
    if (GLAD_GL_KHR_debug) {
        glEnable(GL_DEBUG_OUTPUT);
        glDebugMessageCallback(DebugHandler, nullptr);
    }

    AddTelemetryFields();

    if (!GLAD_GL_VERSION_4_3) {
        return false;
    }

    InitOpenGLObjects();
    CreateRasterizer();

    return true;
}

void RendererOpenGL::ShutDown() {}

} // namespace OpenGL
