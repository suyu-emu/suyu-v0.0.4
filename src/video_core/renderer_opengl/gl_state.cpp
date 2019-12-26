// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <iterator>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "video_core/renderer_opengl/gl_state.h"

MICROPROFILE_DEFINE(OpenGL_State, "OpenGL", "State Change", MP_RGB(192, 128, 128));

namespace OpenGL {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

OpenGLState OpenGLState::cur_state;

namespace {

template <typename T>
bool UpdateValue(T& current_value, const T new_value) {
    const bool changed = current_value != new_value;
    current_value = new_value;
    return changed;
}

template <typename T1, typename T2>
bool UpdateTie(T1 current_value, const T2 new_value) {
    const bool changed = current_value != new_value;
    current_value = new_value;
    return changed;
}

template <typename T>
std::optional<std::pair<GLuint, GLsizei>> UpdateArray(T& current_values, const T& new_values) {
    std::optional<std::size_t> first;
    std::size_t last;
    for (std::size_t i = 0; i < std::size(current_values); ++i) {
        if (!UpdateValue(current_values[i], new_values[i])) {
            continue;
        }
        if (!first) {
            first = i;
        }
        last = i;
    }
    if (!first) {
        return std::nullopt;
    }
    return std::make_pair(static_cast<GLuint>(*first), static_cast<GLsizei>(last - *first + 1));
}

void Enable(GLenum cap, bool enable) {
    if (enable) {
        glEnable(cap);
    } else {
        glDisable(cap);
    }
}

void Enable(GLenum cap, GLuint index, bool enable) {
    if (enable) {
        glEnablei(cap, index);
    } else {
        glDisablei(cap, index);
    }
}

void Enable(GLenum cap, bool& current_value, bool new_value) {
    if (UpdateValue(current_value, new_value)) {
        Enable(cap, new_value);
    }
}

void Enable(GLenum cap, GLuint index, bool& current_value, bool new_value) {
    if (UpdateValue(current_value, new_value)) {
        Enable(cap, index, new_value);
    }
}

} // Anonymous namespace

OpenGLState::OpenGLState() = default;

void OpenGLState::ApplyShaderProgram() {
    if (UpdateValue(cur_state.draw.shader_program, draw.shader_program)) {
        glUseProgram(draw.shader_program);
    }
}

void OpenGLState::ApplyProgramPipeline() {
    if (UpdateValue(cur_state.draw.program_pipeline, draw.program_pipeline)) {
        glBindProgramPipeline(draw.program_pipeline);
    }
}

void OpenGLState::Apply() {
    MICROPROFILE_SCOPE(OpenGL_State);
    ApplyShaderProgram();
    ApplyProgramPipeline();
}

OpenGLState& OpenGLState::ResetProgram(GLuint handle) {
    if (draw.shader_program == handle) {
        draw.shader_program = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetPipeline(GLuint handle) {
    if (draw.program_pipeline == handle) {
        draw.program_pipeline = 0;
    }
    return *this;
}

} // namespace OpenGL
