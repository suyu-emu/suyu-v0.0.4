// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "video_core/framebuffer_config.h"
#include "video_core/renderer_opengl/gl_blit_screen.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"
#include "video_core/renderer_opengl/gl_shader_util.h"
#include "video_core/renderer_opengl/gl_state_tracker.h"
#include "video_core/renderer_opengl/present/filters.h"
#include "video_core/renderer_opengl/present/fsr.h"
#include "video_core/renderer_opengl/present/fxaa.h"
#include "video_core/renderer_opengl/present/smaa.h"
#include "video_core/renderer_opengl/present/window_adapt_pass.h"
#include "video_core/textures/decoders.h"

namespace OpenGL {

BlitScreen::BlitScreen(RasterizerOpenGL& rasterizer_,
                       Tegra::MaxwellDeviceMemoryManager& device_memory_,
                       StateTracker& state_tracker_, ProgramManager& program_manager_,
                       Device& device_)
    : rasterizer(rasterizer_), device_memory(device_memory_), state_tracker(state_tracker_),
      program_manager(program_manager_), device(device_) {
    // Allocate textures for the screen
    framebuffer_texture.resource.Create(GL_TEXTURE_2D);

    const GLuint texture = framebuffer_texture.resource.handle;
    glTextureStorage2D(texture, 1, GL_RGBA8, 1, 1);

    // Clear screen to black
    const u8 framebuffer_data[4] = {0, 0, 0, 0};
    glClearTexImage(framebuffer_texture.resource.handle, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                    framebuffer_data);
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

    fxaa.reset();
    smaa.reset();
}

void BlitScreen::DrawScreen(const Tegra::FramebufferConfig& framebuffer,
                            const Layout::FramebufferLayout& layout) {
    FramebufferTextureInfo info = PrepareRenderTarget(framebuffer);
    auto crop = Tegra::NormalizeCrop(framebuffer, info.width, info.height);

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

    GLint old_read_fb;
    GLint old_draw_fb;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &old_read_fb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &old_draw_fb);

    GLuint texture = info.display_texture;

    auto anti_aliasing = Settings::values.anti_aliasing.GetValue();
    if (anti_aliasing != Settings::AntiAliasing::None) {
        glEnablei(GL_SCISSOR_TEST, 0);
        auto scissor_width = Settings::values.resolution_info.ScaleUp(framebuffer_texture.width);
        auto viewport_width = static_cast<GLfloat>(scissor_width);
        auto scissor_height = Settings::values.resolution_info.ScaleUp(framebuffer_texture.height);
        auto viewport_height = static_cast<GLfloat>(scissor_height);

        glScissorIndexed(0, 0, 0, scissor_width, scissor_height);
        glViewportIndexedf(0, 0.0f, 0.0f, viewport_width, viewport_height);

        switch (anti_aliasing) {
        case Settings::AntiAliasing::Fxaa:
            CreateFXAA();
            texture = fxaa->Draw(program_manager, info.display_texture);
            break;
        case Settings::AntiAliasing::Smaa:
        default:
            CreateSMAA();
            texture = smaa->Draw(program_manager, info.display_texture);
            break;
        }
    }

    glDisablei(GL_SCISSOR_TEST, 0);

    if (Settings::values.scaling_filter.GetValue() == Settings::ScalingFilter::Fsr) {
        if (!fsr || fsr->NeedsRecreation(layout.screen)) {
            fsr = std::make_unique<FSR>(layout.screen.GetWidth(), layout.screen.GetHeight());
        }

        texture = fsr->Draw(program_manager, texture, info.scaled_width, info.scaled_height, crop);
        crop = {0, 0, 1, 1};
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, old_read_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, old_draw_fb);

    CreateWindowAdapt();
    window_adapt->DrawToFramebuffer(program_manager, texture, layout, crop);

    // TODO
    // program_manager.RestoreGuestPipeline();
}

void BlitScreen::CreateFXAA() {
    smaa.reset();
    if (!fxaa) {
        fxaa = std::make_unique<FXAA>(
            Settings::values.resolution_info.ScaleUp(framebuffer_texture.width),
            Settings::values.resolution_info.ScaleUp(framebuffer_texture.height));
    }
}

void BlitScreen::CreateSMAA() {
    fxaa.reset();
    if (!smaa) {
        smaa = std::make_unique<SMAA>(
            Settings::values.resolution_info.ScaleUp(framebuffer_texture.width),
            Settings::values.resolution_info.ScaleUp(framebuffer_texture.height));
    }
}

void BlitScreen::CreateWindowAdapt() {
    if (window_adapt && Settings::values.scaling_filter.GetValue() == current_window_adapt) {
        return;
    }

    current_window_adapt = Settings::values.scaling_filter.GetValue();
    switch (current_window_adapt) {
    case Settings::ScalingFilter::NearestNeighbor:
        window_adapt = MakeNearestNeighbor(device);
        break;
    case Settings::ScalingFilter::Bicubic:
        window_adapt = MakeBicubic(device);
        break;
    case Settings::ScalingFilter::Gaussian:
        window_adapt = MakeGaussian(device);
        break;
    case Settings::ScalingFilter::ScaleForce:
        window_adapt = MakeScaleForce(device);
        break;
    case Settings::ScalingFilter::Fsr:
    case Settings::ScalingFilter::Bilinear:
    default:
        window_adapt = MakeBilinear(device);
        break;
    }
}

} // namespace OpenGL
