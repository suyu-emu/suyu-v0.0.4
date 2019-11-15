// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <fmt/format.h>
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/shader/shader_ir.h"

namespace OpenGL::GLShader {

using Tegra::Engines::Maxwell3D;
using VideoCommon::Shader::CompileDepth;
using VideoCommon::Shader::CompilerSettings;
using VideoCommon::Shader::ProgramCode;
using VideoCommon::Shader::ShaderIR;

std::string GenerateVertexShader(const Device& device, const ShaderIR& ir, const ShaderIR* ir_b) {
    std::string out = GetCommonDeclarations();
    out += R"(
layout (std140, binding = EMULATION_UBO_BINDING) uniform vs_config {
    float y_direction;
};

)";
    const auto stage = ir_b ? ProgramType::VertexA : ProgramType::VertexB;
    out += Decompile(device, ir, stage, "vertex");
    if (ir_b) {
        out += Decompile(device, *ir_b, ProgramType::VertexB, "vertex_b");
    }

    out += R"(
void main() {
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
    out += R"(
layout (std140, binding = EMULATION_UBO_BINDING) uniform gs_config {
    float y_direction;
};

)";
    out += Decompile(device, ir, ProgramType::Geometry, "geometry");

    out += R"(
void main() {
    execute_geometry();
}
)";
    return out;
}

std::string GenerateFragmentShader(const Device& device, const ShaderIR& ir) {
    std::string out = GetCommonDeclarations();
    out += R"(
layout (location = 0) out vec4 FragColor0;
layout (location = 1) out vec4 FragColor1;
layout (location = 2) out vec4 FragColor2;
layout (location = 3) out vec4 FragColor3;
layout (location = 4) out vec4 FragColor4;
layout (location = 5) out vec4 FragColor5;
layout (location = 6) out vec4 FragColor6;
layout (location = 7) out vec4 FragColor7;

layout (std140, binding = EMULATION_UBO_BINDING) uniform fs_config {
    float y_direction;
};

)";
    out += Decompile(device, ir, ProgramType::Fragment, "fragment");

    out += R"(
void main() {
    execute_fragment();
}
)";
    return out;
}

std::string GenerateComputeShader(const Device& device, const ShaderIR& ir) {
    std::string out = GetCommonDeclarations();
    out += Decompile(device, ir, ProgramType::Compute, "compute");
    out += R"(
void main() {
    execute_compute();
}
)";
    return out;
}

} // namespace OpenGL::GLShader
