// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/framebuffer_config.h"
#include "video_core/host_shaders/ffx_a_h.h"
#include "video_core/host_shaders/ffx_fsr1_h.h"
#include "video_core/host_shaders/full_screen_triangle_vert.h"
#include "video_core/host_shaders/opengl_fidelityfx_fsr_easu_frag.h"
#include "video_core/host_shaders/opengl_fidelityfx_fsr_frag.h"
#include "video_core/host_shaders/opengl_fidelityfx_fsr_rcas_frag.h"
#include "video_core/host_shaders/opengl_present_frag.h"
#include "video_core/host_shaders/opengl_present_scaleforce_frag.h"
#include "video_core/host_shaders/opengl_present_vert.h"
#include "video_core/host_shaders/present_bicubic_frag.h"
#include "video_core/host_shaders/present_gaussian_frag.h"

#include "video_core/renderer_opengl/gl_blit_screen.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/present/fsr.h"
#include "video_core/renderer_opengl/present/fxaa.h"
#include "video_core/renderer_opengl/present/smaa.h"
#include "video_core/textures/decoders.h"

namespace OpenGL {

namespace {
constexpr GLint PositionLocation = 0;
constexpr GLint TexCoordLocation = 1;
constexpr GLint ModelViewMatrixLocation = 0;

struct ScreenRectVertex {
    constexpr ScreenRectVertex(u32 x, u32 y, GLfloat u, GLfloat v)
        : position{{static_cast<GLfloat>(x), static_cast<GLfloat>(y)}}, tex_coord{{u, v}} {}

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
} // namespace

BlitScreen::BlitScreen(RasterizerOpenGL& rasterizer_,
                       Tegra::MaxwellDeviceMemoryManager& device_memory_,
                       StateTracker& state_tracker_, ProgramManager& program_manager_,
                       Device& device_)
    : rasterizer(rasterizer_), device_memory(device_memory_), state_tracker(state_tracker_),
      program_manager(program_manager_), device(device_) {
    // Create shader programs
    const auto replace_include = [](std::string& shader_source, std::string_view include_name,
                                    std::string_view include_content) {
        const std::string include_string = fmt::format("#include \"{}\"", include_name);
        const std::size_t pos = shader_source.find(include_string);
        ASSERT(pos != std::string::npos);
        shader_source.replace(pos, include_string.size(), include_content);
    };

    present_vertex = CreateProgram(HostShaders::OPENGL_PRESENT_VERT, GL_VERTEX_SHADER);
    present_bilinear_fragment = CreateProgram(HostShaders::OPENGL_PRESENT_FRAG, GL_FRAGMENT_SHADER);
    present_bicubic_fragment = CreateProgram(HostShaders::PRESENT_BICUBIC_FRAG, GL_FRAGMENT_SHADER);
    present_gaussian_fragment =
        CreateProgram(HostShaders::PRESENT_GAUSSIAN_FRAG, GL_FRAGMENT_SHADER);
    present_scaleforce_fragment =
        CreateProgram(fmt::format("#version 460\n{}", HostShaders::OPENGL_PRESENT_SCALEFORCE_FRAG),
                      GL_FRAGMENT_SHADER);

    std::string fsr_source{HostShaders::OPENGL_FIDELITYFX_FSR_FRAG};
    replace_include(fsr_source, "ffx_a.h", HostShaders::FFX_A_H);
    replace_include(fsr_source, "ffx_fsr1.h", HostShaders::FFX_FSR1_H);

    std::string fsr_easu_frag_source{HostShaders::OPENGL_FIDELITYFX_FSR_EASU_FRAG};
    std::string fsr_rcas_frag_source{HostShaders::OPENGL_FIDELITYFX_FSR_RCAS_FRAG};
    replace_include(fsr_easu_frag_source, "opengl_fidelityfx_fsr.frag", fsr_source);
    replace_include(fsr_rcas_frag_source, "opengl_fidelityfx_fsr.frag", fsr_source);

    fsr = std::make_unique<FSR>(HostShaders::FULL_SCREEN_TRIANGLE_VERT, fsr_easu_frag_source,
                                fsr_rcas_frag_source);

    // Generate presentation sampler
    present_sampler.Create();
    glSamplerParameteri(present_sampler.handle, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glSamplerParameteri(present_sampler.handle, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glSamplerParameteri(present_sampler.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(present_sampler.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(present_sampler.handle, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    present_sampler_nn.Create();
    glSamplerParameteri(present_sampler_nn.handle, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glSamplerParameteri(present_sampler_nn.handle, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glSamplerParameteri(present_sampler_nn.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(present_sampler_nn.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(present_sampler_nn.handle, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    // Generate VBO handle for drawing
    vertex_buffer.Create();

    // Attach vertex data to VAO
    glNamedBufferData(vertex_buffer.handle, sizeof(ScreenRectVertex) * 4, nullptr, GL_STREAM_DRAW);

    // Allocate textures for the screen
    framebuffer_texture.resource.Create(GL_TEXTURE_2D);

    const GLuint texture = framebuffer_texture.resource.handle;
    glTextureStorage2D(texture, 1, GL_RGBA8, 1, 1);

    // Clear screen to black
    const u8 framebuffer_data[4] = {0, 0, 0, 0};
    glClearTexImage(framebuffer_texture.resource.handle, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    framebuffer_data);

    // Enable unified vertex attributes and query vertex buffer address when the driver supports it
    if (device.HasVertexBufferUnifiedMemory()) {
        glEnableClientState(GL_VERTEX_ATTRIB_ARRAY_UNIFIED_NV);
        glEnableClientState(GL_ELEMENT_ARRAY_UNIFIED_NV);
        glMakeNamedBufferResidentNV(vertex_buffer.handle, GL_READ_ONLY);
        glGetNamedBufferParameterui64vNV(vertex_buffer.handle, GL_BUFFER_GPU_ADDRESS_NV,
                                         &vertex_buffer_address);
    }
}

BlitScreen::~BlitScreen() = default;

FramebufferTextureInfo BlitScreen::PrepareRenderTarget(
    const Tegra::FramebufferConfig& framebuffer) {
    // If framebuffer is provided, reload it from memory to a texture
    if (framebuffer_texture.width != static_cast<GLsizei>(framebuffer.width) ||
        framebuffer_texture.height != static_cast<GLsizei>(framebuffer.height) ||
        framebuffer_texture.pixel_format != framebuffer.pixel_format ||
        gl_framebuffer_data.empty()) {
        // Reallocate texture if the framebuffer size has changed.
        // This is expected to not happen very often and hence should not be a
        // performance problem.
        ConfigureFramebufferTexture(framebuffer);
    }

    // Load the framebuffer from memory if needed
    return LoadFBToScreenInfo(framebuffer);
}

FramebufferTextureInfo BlitScreen::LoadFBToScreenInfo(const Tegra::FramebufferConfig& framebuffer) {
    const DAddr framebuffer_addr{framebuffer.address + framebuffer.offset};
    const auto accelerated_info =
        rasterizer.AccelerateDisplay(framebuffer, framebuffer_addr, framebuffer.stride);
    if (accelerated_info) {
        return *accelerated_info;
    }

    // Reset the screen info's display texture to its own permanent texture
    FramebufferTextureInfo info{};
    info.display_texture = framebuffer_texture.resource.handle;
    info.width = framebuffer.width;
    info.height = framebuffer.height;
    info.scaled_width = framebuffer.width;
    info.scaled_height = framebuffer.height;

    // TODO(Rodrigo): Read this from HLE
    constexpr u32 block_height_log2 = 4;
    const auto pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format)};
    const u32 bytes_per_pixel{VideoCore::Surface::BytesPerBlock(pixel_format)};
    const u64 size_in_bytes{Tegra::Texture::CalculateSize(
        true, bytes_per_pixel, framebuffer.stride, framebuffer.height, 1, block_height_log2, 0)};
    const u8* const host_ptr{device_memory.GetPointer<u8>(framebuffer_addr)};
    const std::span<const u8> input_data(host_ptr, size_in_bytes);
    Tegra::Texture::UnswizzleTexture(gl_framebuffer_data, input_data, bytes_per_pixel,
                                     framebuffer.width, framebuffer.height, 1, block_height_log2,
                                     0);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(framebuffer.stride));

    // Update existing texture
    // TODO: Test what happens on hardware when you change the framebuffer dimensions so that
    //       they differ from the LCD resolution.
    // TODO: Applications could theoretically crash yuzu here by specifying too large
    //       framebuffer sizes. We should make sure that this cannot happen.
    glTextureSubImage2D(framebuffer_texture.resource.handle, 0, 0, 0, framebuffer.width,
                        framebuffer.height, framebuffer_texture.gl_format,
                        framebuffer_texture.gl_type, gl_framebuffer_data.data());

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    return info;
}

void BlitScreen::ConfigureFramebufferTexture(const Tegra::FramebufferConfig& framebuffer) {
    framebuffer_texture.width = framebuffer.width;
    framebuffer_texture.height = framebuffer.height;
    framebuffer_texture.pixel_format = framebuffer.pixel_format;

    const auto pixel_format{
        VideoCore::Surface::PixelFormatFromGPUPixelFormat(framebuffer.pixel_format)};
    const u32 bytes_per_pixel{VideoCore::Surface::BytesPerBlock(pixel_format)};
    gl_framebuffer_data.resize(framebuffer_texture.width * framebuffer_texture.height *
                               bytes_per_pixel);

    GLint internal_format;
    switch (framebuffer.pixel_format) {
    case Service::android::PixelFormat::Rgba8888:
        internal_format = GL_RGBA8;
        framebuffer_texture.gl_format = GL_RGBA;
        framebuffer_texture.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        break;
    case Service::android::PixelFormat::Rgb565:
        internal_format = GL_RGB565;
        framebuffer_texture.gl_format = GL_RGB;
        framebuffer_texture.gl_type = GL_UNSIGNED_SHORT_5_6_5;
        break;
    default:
        internal_format = GL_RGBA8;
        framebuffer_texture.gl_format = GL_RGBA;
        framebuffer_texture.gl_type = GL_UNSIGNED_INT_8_8_8_8_REV;
        // UNIMPLEMENTED_MSG("Unknown framebuffer pixel format: {}",
        //                   static_cast<u32>(framebuffer.pixel_format));
        break;
    }

    framebuffer_texture.resource.Release();
    framebuffer_texture.resource.Create(GL_TEXTURE_2D);
    glTextureStorage2D(framebuffer_texture.resource.handle, 1, internal_format,
                       framebuffer_texture.width, framebuffer_texture.height);

    fxaa = std::make_unique<FXAA>(
        Settings::values.resolution_info.ScaleUp(framebuffer_texture.width),
        Settings::values.resolution_info.ScaleUp(framebuffer_texture.height));
    smaa = std::make_unique<SMAA>(
        Settings::values.resolution_info.ScaleUp(framebuffer_texture.width),
        Settings::values.resolution_info.ScaleUp(framebuffer_texture.height));
}

void BlitScreen::DrawScreen(const Tegra::FramebufferConfig& framebuffer,
                            const Layout::FramebufferLayout& layout) {
    FramebufferTextureInfo info = PrepareRenderTarget(framebuffer);
    const auto crop = Tegra::NormalizeCrop(framebuffer, info.width, info.height);

    // TODO: Signal state tracker about these changes
    state_tracker.NotifyScreenDrawVertexArray();
    state_tracker.NotifyPolygonModes();
    state_tracker.NotifyViewport0();
    state_tracker.NotifyScissor0();
    state_tracker.NotifyColorMask(0);
    state_tracker.NotifyBlend0();
    state_tracker.NotifyFramebuffer();
    state_tracker.NotifyFrontFace();
    state_tracker.NotifyCullTest();
    state_tracker.NotifyDepthTest();
    state_tracker.NotifyStencilTest();
    state_tracker.NotifyPolygonOffset();
    state_tracker.NotifyRasterizeEnable();
    state_tracker.NotifyFramebufferSRGB();
    state_tracker.NotifyLogicOp();
    state_tracker.NotifyClipControl();
    state_tracker.NotifyAlphaTest();

    state_tracker.ClipControl(GL_LOWER_LEFT, GL_ZERO_TO_ONE);

    glEnable(GL_CULL_FACE);
    glDisable(GL_COLOR_LOGIC_OP);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_POLYGON_OFFSET_FILL);
    glDisable(GL_RASTERIZER_DISCARD);
    glDisable(GL_ALPHA_TEST);
    glDisablei(GL_BLEND, 0);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);
    glColorMaski(0, GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthRangeIndexed(0, 0.0, 0.0);

    glBindTextureUnit(0, info.display_texture);

    auto anti_aliasing = Settings::values.anti_aliasing.GetValue();
    if (anti_aliasing >= Settings::AntiAliasing::MaxEnum) {
        LOG_ERROR(Render_OpenGL, "Invalid antialiasing option selected {}", anti_aliasing);
        anti_aliasing = Settings::AntiAliasing::None;
        Settings::values.anti_aliasing.SetValue(anti_aliasing);
    }

    if (anti_aliasing != Settings::AntiAliasing::None) {
        glEnablei(GL_SCISSOR_TEST, 0);
        auto scissor_width = Settings::values.resolution_info.ScaleUp(framebuffer_texture.width);
        auto viewport_width = static_cast<GLfloat>(scissor_width);
        auto scissor_height = Settings::values.resolution_info.ScaleUp(framebuffer_texture.height);
        auto viewport_height = static_cast<GLfloat>(scissor_height);

        glScissorIndexed(0, 0, 0, scissor_width, scissor_height);
        glViewportIndexedf(0, 0.0f, 0.0f, viewport_width, viewport_height);

        glBindSampler(0, present_sampler.handle);
        GLint old_read_fb;
        GLint old_draw_fb;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_fb);
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw_fb);

        switch (anti_aliasing) {
        case Settings::AntiAliasing::Fxaa: {
            glBindTextureUnit(0, fxaa->Draw(program_manager, info.display_texture));
        } break;
        case Settings::AntiAliasing::Smaa: {
            glBindTextureUnit(0, smaa->Draw(program_manager, info.display_texture));
        } break;
        default:
            UNREACHABLE();
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, old_read_fb);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw_fb);
    }
    glDisablei(GL_SCISSOR_TEST, 0);

    if (Settings::values.scaling_filter.GetValue() == Settings::ScalingFilter::Fsr) {
        if (!fsr->AreBuffersInitialized()) {
            fsr->InitBuffers();
        }

        glBindSampler(0, present_sampler.handle);
        fsr->Draw(program_manager, layout.screen, info.scaled_width, info.scaled_height, crop);
    } else {
        if (fsr->AreBuffersInitialized()) {
            fsr->ReleaseBuffers();
        }
    }

    const std::array ortho_matrix =
        MakeOrthographicMatrix(static_cast<float>(layout.width), static_cast<float>(layout.height));

    const auto fragment_handle = [this]() {
        switch (Settings::values.scaling_filter.GetValue()) {
        case Settings::ScalingFilter::NearestNeighbor:
        case Settings::ScalingFilter::Bilinear:
            return present_bilinear_fragment.handle;
        case Settings::ScalingFilter::Bicubic:
            return present_bicubic_fragment.handle;
        case Settings::ScalingFilter::Gaussian:
            return present_gaussian_fragment.handle;
        case Settings::ScalingFilter::ScaleForce:
            return present_scaleforce_fragment.handle;
        case Settings::ScalingFilter::Fsr:
            return fsr->GetPresentFragmentProgram().handle;
        default:
            return present_bilinear_fragment.handle;
        }
    }();
    program_manager.BindPresentPrograms(present_vertex.handle, fragment_handle);
    glProgramUniformMatrix3x2fv(present_vertex.handle, ModelViewMatrixLocation, 1, GL_FALSE,
                                ortho_matrix.data());

    f32 left, top, right, bottom;
    if (Settings::values.scaling_filter.GetValue() == Settings::ScalingFilter::Fsr) {
        // FSR has already applied the crop, so we just want to render the image
        // it has produced.
        left = 0;
        top = 0;
        right = 1;
        bottom = 1;
    } else {
        // Apply the precomputed crop.
        left = crop.left;
        top = crop.top;
        right = crop.right;
        bottom = crop.bottom;
    }

    // Map the coordinates to the screen.
    const auto& screen = layout.screen;
    const auto x = screen.left;
    const auto y = screen.top;
    const auto w = screen.GetWidth();
    const auto h = screen.GetHeight();

    const std::array vertices = {
        ScreenRectVertex(x, y, left, top),
        ScreenRectVertex(x + w, y, right, top),
        ScreenRectVertex(x, y + h, left, bottom),
        ScreenRectVertex(x + w, y + h, right, bottom),
    };
    glNamedBufferSubData(vertex_buffer.handle, 0, sizeof(vertices), std::data(vertices));

    glDisable(GL_FRAMEBUFFER_SRGB);
    glViewportIndexedf(0, 0.0f, 0.0f, static_cast<GLfloat>(layout.width),
                       static_cast<GLfloat>(layout.height));

    glEnableVertexAttribArray(PositionLocation);
    glEnableVertexAttribArray(TexCoordLocation);
    glVertexAttribDivisor(PositionLocation, 0);
    glVertexAttribDivisor(TexCoordLocation, 0);
    glVertexAttribFormat(PositionLocation, 2, GL_FLOAT, GL_FALSE,
                         offsetof(ScreenRectVertex, position));
    glVertexAttribFormat(TexCoordLocation, 2, GL_FLOAT, GL_FALSE,
                         offsetof(ScreenRectVertex, tex_coord));
    glVertexAttribBinding(PositionLocation, 0);
    glVertexAttribBinding(TexCoordLocation, 0);
    if (device.HasVertexBufferUnifiedMemory()) {
        glBindVertexBuffer(0, 0, 0, sizeof(ScreenRectVertex));
        glBufferAddressRangeNV(GL_VERTEX_ATTRIB_ARRAY_ADDRESS_NV, 0, vertex_buffer_address,
                               sizeof(vertices));
    } else {
        glBindVertexBuffer(0, vertex_buffer.handle, 0, sizeof(ScreenRectVertex));
    }

    if (Settings::values.scaling_filter.GetValue() != Settings::ScalingFilter::NearestNeighbor) {
        glBindSampler(0, present_sampler.handle);
    } else {
        glBindSampler(0, present_sampler_nn.handle);
    }

    // Update background color before drawing
    glClearColor(Settings::values.bg_red.GetValue() / 255.0f,
                 Settings::values.bg_green.GetValue() / 255.0f,
                 Settings::values.bg_blue.GetValue() / 255.0f, 1.0f);

    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // TODO
    // program_manager.RestoreGuestPipeline();
}

} // namespace OpenGL
