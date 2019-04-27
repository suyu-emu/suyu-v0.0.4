// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::Instruction;
using Tegra::Shader::OpCode;

namespace {
std::size_t GetImageTypeNumCoordinates(Tegra::Shader::ImageType image_type) {
    switch (image_type) {
    case Tegra::Shader::ImageType::Texture1D:
    case Tegra::Shader::ImageType::TextureBuffer:
        return 1;
    case Tegra::Shader::ImageType::Texture1DArray:
    case Tegra::Shader::ImageType::Texture2D:
        return 2;
    case Tegra::Shader::ImageType::Texture2DArray:
    case Tegra::Shader::ImageType::Texture3D:
        return 3;
    }
    UNREACHABLE();
    return 1;
}
} // Anonymous namespace

u32 ShaderIR::DecodeImage(NodeBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    switch (opcode->get().GetId()) {
    case OpCode::Id::SUST: {
        UNIMPLEMENTED_IF(instr.sust.mode != Tegra::Shader::SurfaceDataMode::P);
        UNIMPLEMENTED_IF(instr.sust.image_type == Tegra::Shader::ImageType::TextureBuffer);
        UNIMPLEMENTED_IF(instr.sust.out_of_bounds_store != Tegra::Shader::OutOfBoundsStore::Ignore);
        UNIMPLEMENTED_IF(instr.sust.component_mask_selector != 0xf); // Ensure we have an RGBA store

        std::vector<Node> values;
        constexpr std::size_t hardcoded_size{4};
        for (std::size_t i = 0; i < hardcoded_size; ++i) {
            values.push_back(GetRegister(instr.gpr0.Value() + i));
        }

        std::vector<Node> coords;
        const std::size_t num_coords{GetImageTypeNumCoordinates(instr.sust.image_type)};
        for (std::size_t i = 0; i < num_coords; ++i) {
            coords.push_back(GetRegister(instr.gpr8.Value() + i));
        }

        ASSERT(instr.sust.is_immediate);
        const auto& image{GetImage(instr.image, instr.sust.image_type)};
        MetaImage meta{image, values};
        const Node store{Operation(OperationCode::ImageStore, meta, std::move(coords))};
        bb.push_back(store);
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled conversion instruction: {}", opcode->get().GetName());
    }

    return pc;
}

const Image& ShaderIR::GetImage(Tegra::Shader::Image image, Tegra::Shader::ImageType type) {
    const auto offset{static_cast<std::size_t>(image.index.Value())};

    // If this image has already been used, return the existing mapping.
    const auto itr{std::find_if(used_images.begin(), used_images.end(),
                                [=](const Image& entry) { return entry.GetOffset() == offset; })};
    if (itr != used_images.end()) {
        ASSERT(itr->GetType() == type);
        return *itr;
    }

    // Otherwise create a new mapping for this image.
    const std::size_t next_index{used_images.size()};
    const Image entry{offset, next_index, type};
    return *used_images.emplace(entry).first;
}

} // namespace VideoCommon::Shader
