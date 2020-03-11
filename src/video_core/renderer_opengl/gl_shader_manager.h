// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/maxwell_to_gl.h"

namespace OpenGL::GLShader {

/// Uniform structure for the Uniform Buffer Object, all vectors must be 16-byte aligned
/// @note Always keep a vec4 at the end. The GL spec is not clear whether the alignment at
///       the end of a uniform block is included in UNIFORM_BLOCK_DATA_SIZE or not.
///       Not following that rule will cause problems on some AMD drivers.
struct alignas(16) MaxwellUniformData {
    void SetFromRegs(const Tegra::Engines::Maxwell3D& maxwell);

    GLfloat y_direction;
};
static_assert(sizeof(MaxwellUniformData) == 16, "MaxwellUniformData structure size is incorrect");
static_assert(sizeof(MaxwellUniformData) < 16384,
              "MaxwellUniformData structure must be less than 16kb as per the OpenGL spec");

class ProgramManager {
public:
    explicit ProgramManager();
    ~ProgramManager();

    void Create();

    /// Updates the graphics pipeline and binds it.
    void BindGraphicsPipeline();

    /// Binds a compute shader.
    void BindComputeShader(GLuint program);

    void UseVertexShader(GLuint program) {
        current_state.vertex_shader = program;
    }

    void UseGeometryShader(GLuint program) {
        current_state.geometry_shader = program;
    }

    void UseFragmentShader(GLuint program) {
        current_state.fragment_shader = program;
    }

private:
    struct PipelineState {
        bool operator==(const PipelineState& rhs) const noexcept {
            return vertex_shader == rhs.vertex_shader && fragment_shader == rhs.fragment_shader &&
                   geometry_shader == rhs.geometry_shader;
        }

        bool operator!=(const PipelineState& rhs) const noexcept {
            return !operator==(rhs);
        }

        GLuint vertex_shader = 0;
        GLuint fragment_shader = 0;
        GLuint geometry_shader = 0;
    };

    OGLPipeline graphics_pipeline;
    OGLPipeline compute_pipeline;
    PipelineState current_state;
    PipelineState old_state;
    bool is_graphics_bound = true;
};

} // namespace OpenGL::GLShader
