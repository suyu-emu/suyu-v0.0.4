// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include <sirit/sirit.h>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {
class ShaderIR;
}

namespace Vulkan {
class VKDevice;
}

namespace Vulkan::VKShader {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using SamplerEntry = VideoCommon::Shader::Sampler;

constexpr u32 DESCRIPTOR_SET = 0;

class ConstBufferEntry : public VideoCommon::Shader::ConstBuffer {
public:
    explicit constexpr ConstBufferEntry(const VideoCommon::Shader::ConstBuffer& entry, u32 index)
        : VideoCommon::Shader::ConstBuffer{entry}, index{index} {}

    constexpr u32 GetIndex() const {
        return index;
    }

private:
    u32 index{};
};

class GlobalBufferEntry {
public:
    explicit GlobalBufferEntry(u32 cbuf_index, u32 cbuf_offset)
        : cbuf_index{cbuf_index}, cbuf_offset{cbuf_offset} {}

    u32 GetCbufIndex() const {
        return cbuf_index;
    }

    u32 GetCbufOffset() const {
        return cbuf_offset;
    }

private:
    u32 cbuf_index{};
    u32 cbuf_offset{};
};

struct ShaderEntries {
    u32 const_buffers_base_binding{};
    u32 global_buffers_base_binding{};
    u32 samplers_base_binding{};
    std::vector<ConstBufferEntry> const_buffers;
    std::vector<GlobalBufferEntry> global_buffers;
    std::vector<SamplerEntry> samplers;
    std::set<u32> attributes;
    std::array<bool, Maxwell::NumClipDistances> clip_distances{};
    std::size_t shader_length{};
    Sirit::Id entry_function{};
    std::vector<Sirit::Id> interfaces;
};

using DecompilerResult = std::pair<std::unique_ptr<Sirit::Module>, ShaderEntries>;

DecompilerResult Decompile(const VKDevice& device, const VideoCommon::Shader::ShaderIR& ir,
                           Tegra::Engines::ShaderType stage);

} // namespace Vulkan::VKShader
