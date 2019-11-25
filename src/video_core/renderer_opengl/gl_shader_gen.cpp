// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string>

#include <fmt/format.h>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_device.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/shader/shader_ir.h"

namespace OpenGL::GLShader {

using Tegra::Engines::Maxwell3D;
using Tegra::Engines::ShaderType;
using VideoCommon::Shader::CompileDepth;
using VideoCommon::Shader::CompilerSettings;
using VideoCommon::Shader::ProgramCode;
using VideoCommon::Shader::ShaderIR;

std::string GenerateVertexShader(const Device& device, const ShaderIR& ir, const ShaderIR* ir_b) {
    std::string out = GetCommonDeclarations();
    out += fmt::format(R"(
layout (std140, binding = {}) uniform vs_config {{
    float y_direction;
}};

)",
                       EmulationUniformBlockBinding);
    out += Decompile(device, ir, ShaderType::Vertex, "vertex");
    if (ir_b) {
        out += Decompile(device, *ir_b, ShaderType::Vertex, "vertex_b");
    }

    out += R"(
void main() {
    gl_Position = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    execute_vertex();
)";
    if (ir_b) {
        out += "    execute_vertex_b();";
    }
    out += "}\n";
    return out;
}

std::string GenerateGeometryShader(const Device& device, const ShaderIR& ir) {
    std::string out = GetCommonDeclarations();
    out += fmt::format(R"(
layout (std140, binding = {}) uniform gs_config {{
    float y_direction;
}};

)",
                       EmulationUniformBlockBinding);
    out += Decompile(device, ir, ShaderType::Geometry, "geometry");

    out += R"(
void main() {
    execute_geometry();
}
)";
    return out;
}

std::string GenerateFragmentShader(const Device& device, const ShaderIR& ir) {
    std::string out = GetCommonDeclarations();
    out += fmt::format(R"(
layout (location = 0) out vec4 FragColor0;
layout (location = 1) out vec4 FragColor1;
layout (location = 2) out vec4 FragColor2;
layout (location = 3) out vec4 FragColor3;
layout (location = 4) out vec4 FragColor4;
layout (location = 5) out vec4 FragColor5;
layout (location = 6) out vec4 FragColor6;
layout (location = 7) out vec4 FragColor7;

layout (std140, binding = {}) uniform fs_config {{
    float y_direction;
}};

)",
                       EmulationUniformBlockBinding);
    out += Decompile(device, ir, ShaderType::Fragment, "fragment");

    out += R"(
void main() {
    execute_fragment();
}
)";
    return out;
}

std::string GenerateComputeShader(const Device& device, const ShaderIR& ir) {
    std::string out = GetCommonDeclarations();
    out += Decompile(device, ir, ShaderType::Compute, "compute");
    out += R"(
void main() {
    execute_compute();
}
)";
    return out;
}

} // namespace OpenGL::GLShader
