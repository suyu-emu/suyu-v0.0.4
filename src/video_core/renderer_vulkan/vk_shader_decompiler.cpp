// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sirit/sirit.h>

#include "common/assert.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_header.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"
#include "video_core/shader/shader_ir.h"

namespace Vulkan::VKShader {

using namespace VideoCommon::Shader;

using ShaderStage = Tegra::Engines::Maxwell3D::Regs::ShaderStage;

class SPIRVDecompiler : public Sirit::Module {
public:
    explicit SPIRVDecompiler(const ShaderIR& ir, ShaderStage stage)
        : Module(0x00010300), ir{ir}, stage{stage}, header{ir.GetHeader()} {}

    void Decompile() {
        UNIMPLEMENTED();
    }

    ShaderEntries GetShaderEntries() const {
        UNIMPLEMENTED();
        return {};
    }

private:
    const ShaderIR& ir;
    const ShaderStage stage;
    const Tegra::Shader::Header header;
};

DecompilerResult Decompile(const VideoCommon::Shader::ShaderIR& ir, Maxwell::ShaderStage stage) {
    auto decompiler = std::make_unique<SPIRVDecompiler>(ir, stage);
    decompiler->Decompile();
    return {std::move(decompiler), decompiler->GetShaderEntries()};
}

} // namespace Vulkan::VKShader
