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
    struct {
        GLuint shader_program = 0;   // GL_CURRENT_PROGRAM
        GLuint program_pipeline = 0; // GL_PROGRAM_PIPELINE_BINDING
    } draw;

    OpenGLState();

    /// Get the currently active OpenGL state
    static OpenGLState GetCurState() {
        return cur_state;
    }

    /// Apply this state as the current OpenGL state
    void Apply();

    void ApplyShaderProgram();
    void ApplyProgramPipeline();

    /// Resets any references to the given resource
    OpenGLState& ResetProgram(GLuint handle);
    OpenGLState& ResetPipeline(GLuint handle);

private:
    static OpenGLState cur_state;
};
static_assert(std::is_trivially_copyable_v<OpenGLState>);

} // namespace OpenGL
