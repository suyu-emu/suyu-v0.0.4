// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Layout {
struct FramebufferLayout;
}

namespace OpenGL {

class Device;
class ProgramManager;

class WindowAdaptPass final {
public:
    explicit WindowAdaptPass(const Device& device, OGLSampler&& sampler,
                             std::string_view frag_source);
    ~WindowAdaptPass();

    void DrawToFramebuffer(ProgramManager& program_manager, GLuint texture,
                           const Layout::FramebufferLayout& layout,
                           const Common::Rectangle<f32>& crop);

private:
    const Device& device;
    OGLSampler sampler;
    OGLProgram vert;
    OGLProgram frag;
    OGLBuffer vertex_buffer;

    // GPU address of the vertex buffer
    GLuint64EXT vertex_buffer_address = 0;
};

} // namespace OpenGL
