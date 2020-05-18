// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include <glad/glad.h>

#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/maxwell_to_gl.h"

namespace OpenGL {

class Device;

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
    explicit ProgramManager(const Device& device);
    ~ProgramManager();

    /// Binds a compute program
    void BindCompute(GLuint program);

    /// Updates bound programs.
    void BindGraphicsPipeline();

    /// Binds an OpenGL pipeline object unsynchronized with the guest state.
    void BindHostPipeline(GLuint pipeline);

    /// Rewinds BindHostPipeline state changes.
    void RestoreGuestPipeline();

    void UseVertexShader(GLuint program) {
        current_state.vertex = program;
    }

    void UseGeometryShader(GLuint program) {
        current_state.geometry = program;
    }

    void UseFragmentShader(GLuint program) {
        current_state.fragment = program;
    }

private:
    struct PipelineState {
        GLuint vertex = 0;
        GLuint geometry = 0;
        GLuint fragment = 0;
    };

    /// Update NV_gpu_program5 programs.
    void UpdateAssemblyPrograms();

    /// Update GLSL programs.
    void UpdateSourcePrograms();

    OGLPipeline graphics_pipeline;

    PipelineState current_state;
    PipelineState old_state;

    bool use_assembly_programs = false;

    bool is_graphics_bound = true;

    bool vertex_enabled = false;
    bool geometry_enabled = false;
    bool fragment_enabled = false;
};

} // namespace OpenGL
