// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_opengl/gl_shader_manager.h"

namespace OpenGL::GLShader {

using Tegra::Engines::Maxwell3D;

ProgramManager::ProgramManager() {
    pipeline.Create();
}

ProgramManager::~ProgramManager() = default;

void ProgramManager::ApplyTo(OpenGLState& state) {
    UpdatePipeline();
    state.draw.shader_program = 0;
    state.draw.program_pipeline = pipeline.handle;
}

void ProgramManager::UpdatePipeline() {
    // Avoid updating the pipeline when values have no changed
    if (old_state == current_state) {
        return;
    }

    // Workaround for AMD bug
    constexpr GLenum all_used_stages{GL_VERTEX_SHADER_BIT | GL_GEOMETRY_SHADER_BIT |
                                     GL_FRAGMENT_SHADER_BIT};
    glUseProgramStages(pipeline.handle, all_used_stages, 0);

    glUseProgramStages(pipeline.handle, GL_VERTEX_SHADER_BIT, current_state.vertex_shader);
    glUseProgramStages(pipeline.handle, GL_GEOMETRY_SHADER_BIT, current_state.geometry_shader);
    glUseProgramStages(pipeline.handle, GL_FRAGMENT_SHADER_BIT, current_state.fragment_shader);

    old_state = current_state;
}

void MaxwellUniformData::SetFromRegs(const Maxwell3D& maxwell) {
    const auto& regs = maxwell.regs;

    // Y_NEGATE controls what value S2R returns for the Y_DIRECTION system value.
    y_direction = regs.screen_y_control.y_negate == 0 ? 1.0f : -1.0f;
}

} // namespace OpenGL::GLShader
