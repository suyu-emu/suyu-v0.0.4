// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/shader/shader_ir.h"

namespace OpenGL {
class Device;
}

namespace OpenGL::GLShader {

using VideoCommon::Shader::ProgramCode;
using VideoCommon::Shader::ShaderIR;

/// Generates the GLSL vertex shader program source code for the given VS program
std::string GenerateVertexShader(const Device& device, const ShaderIR& ir, const ShaderIR* ir_b);

/// Generates the GLSL geometry shader program source code for the given GS program
std::string GenerateGeometryShader(const Device& device, const ShaderIR& ir);

/// Generates the GLSL fragment shader program source code for the given FS program
std::string GenerateFragmentShader(const Device& device, const ShaderIR& ir);

/// Generates the GLSL compute shader program source code for the given CS program
std::string GenerateComputeShader(const Device& device, const ShaderIR& ir);

} // namespace OpenGL::GLShader
