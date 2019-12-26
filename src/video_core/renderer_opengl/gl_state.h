// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <type_traits>
#include <glad/glad.h>
#include "video_core/engines/maxwell_3d.h"

namespace OpenGL {

class OpenGLState {
public:
    static constexpr std::size_t NumSamplers = 32 * 5;
    static constexpr std::size_t NumImages = 8 * 5;
    std::array<GLuint, NumSamplers> textures = {};
    std::array<GLuint, NumSamplers> samplers = {};
    std::array<GLuint, NumImages> images = {};

    struct {
        GLuint read_framebuffer = 0; // GL_READ_FRAMEBUFFER_BINDING
        GLuint draw_framebuffer = 0; // GL_DRAW_FRAMEBUFFER_BINDING
        GLuint shader_program = 0;   // GL_CURRENT_PROGRAM
        GLuint program_pipeline = 0; // GL_PROGRAM_PIPELINE_BINDING
    } draw;

    GLuint renderbuffer{}; // GL_RENDERBUFFER_BINDING

    OpenGLState();

    /// Get the currently active OpenGL state
    static OpenGLState GetCurState() {
        return cur_state;
    }

    /// Apply this state as the current OpenGL state
    void Apply();

    void ApplyFramebufferState();
    void ApplyShaderProgram();
    void ApplyProgramPipeline();
    void ApplyTextures();
    void ApplySamplers();
    void ApplyImages();
    void ApplyRenderBuffer();

    /// Resets any references to the given resource
    OpenGLState& UnbindTexture(GLuint handle);
    OpenGLState& ResetSampler(GLuint handle);
    OpenGLState& ResetProgram(GLuint handle);
    OpenGLState& ResetPipeline(GLuint handle);
    OpenGLState& ResetFramebuffer(GLuint handle);
    OpenGLState& ResetRenderbuffer(GLuint handle);

private:
    static OpenGLState cur_state;
};
static_assert(std::is_trivially_copyable_v<OpenGLState>);

} // namespace OpenGL
