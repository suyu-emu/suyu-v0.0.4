// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {
class ShaderIR;
}

namespace OpenGL::GLShader {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

class ConstBufferEntry : public VideoCommon::Shader::ConstBuffer {
public:
    explicit ConstBufferEntry(const VideoCommon::Shader::ConstBuffer& entry,
                              Maxwell::ShaderStage stage, const std::string& name, u32 index)
        : VideoCommon::Shader::ConstBuffer{entry}, stage{stage}, name{name}, index{index} {}

    const std::string& GetName() const {
        return name;
    }

    Maxwell::ShaderStage GetStage() const {
        return stage;
    }

    u32 GetIndex() const {
        return index;
    }

    u32 GetHash() const {
        return (static_cast<u32>(stage) << 16) | index;
    }

private:
    std::string name;
    Maxwell::ShaderStage stage{};
    u32 index{};
};

class SamplerEntry : public VideoCommon::Shader::Sampler {
public:
    explicit SamplerEntry(const VideoCommon::Shader::Sampler& entry, Maxwell::ShaderStage stage,
                          const std::string& name)
        : VideoCommon::Shader::Sampler{entry}, stage{stage}, name{name} {}

    const std::string& GetName() const {
        return name;
    }

    Maxwell::ShaderStage GetStage() const {
        return stage;
    }

    u32 GetHash() const {
        return (static_cast<u32>(stage) << 16) | static_cast<u32>(GetIndex());
    }

private:
    std::string name;
    Maxwell::ShaderStage stage{};
};

struct ShaderEntries {
    std::vector<ConstBufferEntry> const_buffers;
    std::vector<SamplerEntry> samplers;
    std::array<bool, Maxwell::NumClipDistances> clip_distances{};
    std::size_t shader_length{};
};

using ProgramResult = std::pair<std::string, ShaderEntries>;

std::string GetCommonDeclarations();

ProgramResult Decompile(const VideoCommon::Shader::ShaderIR& ir, Maxwell::ShaderStage stage,
                        const std::string& suffix);

} // namespace OpenGL::GLShader