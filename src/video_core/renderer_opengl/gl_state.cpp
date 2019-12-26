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

void OpenGLState::ApplyFramebufferState() {
    if (UpdateValue(cur_state.draw.read_framebuffer, draw.read_framebuffer)) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, draw.read_framebuffer);
    }
    if (UpdateValue(cur_state.draw.draw_framebuffer, draw.draw_framebuffer)) {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw.draw_framebuffer);
    }
}

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

void OpenGLState::ApplyRenderBuffer() {
    if (cur_state.renderbuffer != renderbuffer) {
        cur_state.renderbuffer = renderbuffer;
        glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
    }
}

void OpenGLState::ApplyTextures() {
    const std::size_t size = std::size(textures);
    for (std::size_t i = 0; i < size; ++i) {
        if (UpdateValue(cur_state.textures[i], textures[i])) {
            // BindTextureUnit doesn't support binding null textures, skip those binds.
            // TODO(Rodrigo): Stop using null textures
            if (textures[i] != 0) {
                glBindTextureUnit(static_cast<GLuint>(i), textures[i]);
            }
        }
    }
}

void OpenGLState::ApplySamplers() {
    const std::size_t size = std::size(samplers);
    for (std::size_t i = 0; i < size; ++i) {
        if (UpdateValue(cur_state.samplers[i], samplers[i])) {
            glBindSampler(static_cast<GLuint>(i), samplers[i]);
        }
    }
}

void OpenGLState::ApplyImages() {
    if (const auto update = UpdateArray(cur_state.images, images)) {
        glBindImageTextures(update->first, update->second, images.data() + update->first);
    }
}

void OpenGLState::Apply() {
    MICROPROFILE_SCOPE(OpenGL_State);
    ApplyFramebufferState();
    ApplyShaderProgram();
    ApplyProgramPipeline();
    ApplyTextures();
    ApplySamplers();
    ApplyImages();
    ApplyRenderBuffer();
}

OpenGLState& OpenGLState::UnbindTexture(GLuint handle) {
    for (auto& texture : textures) {
        if (texture == handle) {
            texture = 0;
        }
    }
    return *this;
}

OpenGLState& OpenGLState::ResetSampler(GLuint handle) {
    for (auto& sampler : samplers) {
        if (sampler == handle) {
            sampler = 0;
        }
    }
    return *this;
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

OpenGLState& OpenGLState::ResetFramebuffer(GLuint handle) {
    if (draw.read_framebuffer == handle) {
        draw.read_framebuffer = 0;
    }
    if (draw.draw_framebuffer == handle) {
        draw.draw_framebuffer = 0;
    }
    return *this;
}

OpenGLState& OpenGLState::ResetRenderbuffer(GLuint handle) {
    if (renderbuffer == handle) {
        renderbuffer = 0;
    }
    return *this;
}

} // namespace OpenGL
