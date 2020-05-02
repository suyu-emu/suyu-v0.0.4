// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/shader/registry.h"
#include "video_core/shader/shader_ir.h"

namespace OpenGL {

class Device;

using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using SamplerEntry = VideoCommon::Shader::Sampler;
using ImageEntry = VideoCommon::Shader::Image;

class ConstBufferEntry : public VideoCommon::Shader::ConstBuffer {
public:
    explicit ConstBufferEntry(u32 max_offset, bool is_indirect, u32 index)
        : VideoCommon::Shader::ConstBuffer{max_offset, is_indirect}, index{index} {}

    u32 GetIndex() const {
        return index;
    }

private:
    u32 index = 0;
};

struct GlobalMemoryEntry {
    constexpr explicit GlobalMemoryEntry(u32 cbuf_index, u32 cbuf_offset, bool is_read,
                                         bool is_written)
        : cbuf_index{cbuf_index}, cbuf_offset{cbuf_offset}, is_read{is_read}, is_written{
                                                                                  is_written} {}

    u32 cbuf_index = 0;
    u32 cbuf_offset = 0;
    bool is_read = false;
    bool is_written = false;
};

struct ShaderEntries {
    std::vector<ConstBufferEntry> const_buffers;
    std::vector<GlobalMemoryEntry> global_memory_entries;
    std::vector<SamplerEntry> samplers;
    std::vector<ImageEntry> images;
    u32 clip_distances{};
    std::size_t shader_length{};
};

ShaderEntries MakeEntries(const VideoCommon::Shader::ShaderIR& ir);

std::string DecompileShader(const Device& device, const VideoCommon::Shader::ShaderIR& ir,
                            const VideoCommon::Shader::Registry& registry,
                            Tegra::Engines::ShaderType stage, std::string_view identifier,
                            std::string_view suffix = {});

} // namespace OpenGL
