// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::Attribute;
using Tegra::Shader::Instruction;
using Tegra::Shader::OpCode;
using Tegra::Shader::Register;
using Tegra::Shader::TextureMiscMode;
using Tegra::Shader::TextureProcessMode;
using Tegra::Shader::TextureType;

static std::size_t GetCoordCount(TextureType texture_type) {
    switch (texture_type) {
    case TextureType::Texture1D:
        return 1;
    case TextureType::Texture2D:
        return 2;
    case TextureType::Texture3D:
    case TextureType::TextureCube:
        return 3;
    default:
        UNIMPLEMENTED_MSG("Unhandled texture type: {}", static_cast<u32>(texture_type));
        return 0;
    }
}

u32 ShaderIR::DecodeMemory(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    switch (opcode->get().GetId()) {
    case OpCode::Id::LD_A: {
        // Note: Shouldn't this be interp mode flat? As in no interpolation made.
        UNIMPLEMENTED_IF_MSG(instr.gpr8.Value() != Register::ZeroIndex,
                             "Indirect attribute loads are not supported");
        UNIMPLEMENTED_IF_MSG((instr.attribute.fmt20.immediate.Value() % sizeof(u32)) != 0,
                             "Unaligned attribute loads are not supported");

        Tegra::Shader::IpaMode input_mode{Tegra::Shader::IpaInterpMode::Perspective,
                                          Tegra::Shader::IpaSampleMode::Default};

        u64 next_element = instr.attribute.fmt20.element;
        auto next_index = static_cast<u64>(instr.attribute.fmt20.index.Value());

        const auto LoadNextElement = [&](u32 reg_offset) {
            const Node buffer = GetRegister(instr.gpr39);
            const Node attribute = GetInputAttribute(static_cast<Attribute::Index>(next_index),
                                                     next_element, input_mode, buffer);

            SetRegister(bb, instr.gpr0.Value() + reg_offset, attribute);

            // Load the next attribute element into the following register. If the element
            // to load goes beyond the vec4 size, load the first element of the next
            // attribute.
            next_element = (next_element + 1) % 4;
            next_index = next_index + (next_element == 0 ? 1 : 0);
        };

        const u32 num_words = static_cast<u32>(instr.attribute.fmt20.size.Value()) + 1;
        for (u32 reg_offset = 0; reg_offset < num_words; ++reg_offset) {
            LoadNextElement(reg_offset);
        }
        break;
    }
    case OpCode::Id::LD_C: {
        UNIMPLEMENTED_IF(instr.ld_c.unknown != 0);

        Node index = GetRegister(instr.gpr8);

        const Node op_a =
            GetConstBufferIndirect(instr.cbuf36.index, instr.cbuf36.offset + 0, index);

        switch (instr.ld_c.type.Value()) {
        case Tegra::Shader::UniformType::Single:
            SetRegister(bb, instr.gpr0, op_a);
            break;

        case Tegra::Shader::UniformType::Double: {
            const Node op_b =
                GetConstBufferIndirect(instr.cbuf36.index, instr.cbuf36.offset + 4, index);

            const Node composite =
                Operation(OperationCode::Composite, op_a, op_b, GetRegister(RZ), GetRegister(RZ));

            MetaComponents meta{{0, 1, 2, 3}};
            bb.push_back(Operation(OperationCode::AssignComposite, meta, composite,
                                   GetRegister(instr.gpr0), GetRegister(instr.gpr0.Value() + 1),
                                   GetRegister(RZ), GetRegister(RZ)));
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unhandled type: {}", static_cast<unsigned>(instr.ld_c.type.Value()));
        }
        break;
    }
    case OpCode::Id::LD_L: {
        UNIMPLEMENTED_IF_MSG(instr.ld_l.unknown == 1, "LD_L Unhandled mode: {}",
                             static_cast<unsigned>(instr.ld_l.unknown.Value()));

        const Node index = Operation(OperationCode::IAdd, GetRegister(instr.gpr8),
                                     Immediate(static_cast<s32>(instr.smem_imm)));
        const Node lmem = GetLocalMemory(index);

        switch (instr.ldst_sl.type.Value()) {
        case Tegra::Shader::StoreType::Bytes32:
            SetRegister(bb, instr.gpr0, lmem);
            break;
        default:
            UNIMPLEMENTED_MSG("LD_L Unhandled type: {}",
                              static_cast<unsigned>(instr.ldst_sl.type.Value()));
        }
        break;
    }
    case OpCode::Id::ST_A: {
        UNIMPLEMENTED_IF_MSG(instr.gpr8.Value() != Register::ZeroIndex,
                             "Indirect attribute loads are not supported");
        UNIMPLEMENTED_IF_MSG((instr.attribute.fmt20.immediate.Value() % sizeof(u32)) != 0,
                             "Unaligned attribute loads are not supported");

        u64 next_element = instr.attribute.fmt20.element;
        auto next_index = static_cast<u64>(instr.attribute.fmt20.index.Value());

        const auto StoreNextElement = [&](u32 reg_offset) {
            const auto dest = GetOutputAttribute(static_cast<Attribute::Index>(next_index),
                                                 next_element, GetRegister(instr.gpr39));
            const auto src = GetRegister(instr.gpr0.Value() + reg_offset);

            bb.push_back(Operation(OperationCode::Assign, dest, src));

            // Load the next attribute element into the following register. If the element
            // to load goes beyond the vec4 size, load the first element of the next
            // attribute.
            next_element = (next_element + 1) % 4;
            next_index = next_index + (next_element == 0 ? 1 : 0);
        };

        const u32 num_words = static_cast<u32>(instr.attribute.fmt20.size.Value()) + 1;
        for (u32 reg_offset = 0; reg_offset < num_words; ++reg_offset) {
            StoreNextElement(reg_offset);
        }

        break;
    }
    case OpCode::Id::ST_L: {
        // UNIMPLEMENTED_IF_MSG(instr.st_l.unknown == 0, "ST_L Unhandled mode: {}",
        //                      static_cast<u32>(instr.st_l.unknown.Value()));

        const Node index = Operation(OperationCode::IAdd, NO_PRECISE, GetRegister(instr.gpr8),
                                     Immediate(static_cast<s32>(instr.smem_imm)));

        switch (instr.ldst_sl.type.Value()) {
        case Tegra::Shader::StoreType::Bytes32:
            SetLocalMemory(bb, index, GetRegister(instr.gpr0));
            break;
        default:
            UNIMPLEMENTED_MSG("ST_L Unhandled type: {}",
                              static_cast<u32>(instr.ldst_sl.type.Value()));
        }
        break;
    }
    case OpCode::Id::TEX: {
        Tegra::Shader::TextureType texture_type{instr.tex.texture_type};
        const bool is_array = instr.tex.array != 0;
        const bool depth_compare = instr.tex.UsesMiscMode(TextureMiscMode::DC);
        const auto process_mode = instr.tex.GetTextureProcessMode();
        UNIMPLEMENTED_IF_MSG(instr.tex.UsesMiscMode(TextureMiscMode::AOFFI),
                             "AOFFI is not implemented");

        if (instr.tex.UsesMiscMode(TextureMiscMode::NODEP)) {
            LOG_WARNING(HW_GPU, "TEX.NODEP implementation is incomplete");
        }

        const Node texture = GetTexCode(instr, texture_type, process_mode, depth_compare, is_array);

        MetaComponents meta;
        std::array<Node, 4> dest;

        std::size_t dest_elem = 0;
        for (std::size_t elem = 0; elem < 4; ++elem) {
            if (!instr.tex.IsComponentEnabled(elem)) {
                // Skip disabled components
                continue;
            }
            meta.components_map[dest_elem] = static_cast<u32>(elem);
            dest[dest_elem] = GetRegister(instr.gpr0.Value() + dest_elem);

            ++dest_elem;
        }
        std::generate(dest.begin() + dest_elem, dest.end(), [&]() { return GetRegister(RZ); });

        bb.push_back(Operation(OperationCode::AssignComposite, std::move(meta), texture, dest[0],
                               dest[1], dest[2], dest[3]));
        break;
    }
    case OpCode::Id::TEXS: {
        Tegra::Shader::TextureType texture_type{instr.texs.GetTextureType()};
        const bool is_array{instr.texs.IsArrayTexture()};
        const bool depth_compare = instr.texs.UsesMiscMode(TextureMiscMode::DC);
        const auto process_mode = instr.texs.GetTextureProcessMode();

        if (instr.texs.UsesMiscMode(TextureMiscMode::NODEP)) {
            LOG_WARNING(HW_GPU, "TEXS.NODEP implementation is incomplete");
        }

        const Node texture =
            GetTexsCode(instr, texture_type, process_mode, depth_compare, is_array);

        if (instr.texs.fp32_flag) {
            WriteTexsInstructionFloat(bb, instr, texture);
        } else {
            UNIMPLEMENTED();
            // WriteTexsInstructionHalfFloat(bb, instr, texture);
        }
        break;
    }
    case OpCode::Id::TLD4: {
        ASSERT(instr.tld4.texture_type == Tegra::Shader::TextureType::Texture2D);
        ASSERT(instr.tld4.array == 0);
        UNIMPLEMENTED_IF_MSG(instr.tld4.UsesMiscMode(TextureMiscMode::AOFFI),
                             "AOFFI is not implemented");
        UNIMPLEMENTED_IF_MSG(instr.tld4.UsesMiscMode(TextureMiscMode::NDV),
                             "NDV is not implemented");
        UNIMPLEMENTED_IF_MSG(instr.tld4.UsesMiscMode(TextureMiscMode::PTP),
                             "PTP is not implemented");

        if (instr.tld4.UsesMiscMode(TextureMiscMode::NODEP)) {
            LOG_WARNING(HW_GPU, "TLD4.NODEP implementation is incomplete");
        }

        const bool depth_compare = instr.tld4.UsesMiscMode(TextureMiscMode::DC);
        auto texture_type = instr.tld4.texture_type.Value();
        u32 num_coordinates = static_cast<u32>(GetCoordCount(texture_type));
        if (depth_compare)
            num_coordinates += 1;

        std::vector<Node> params;

        switch (num_coordinates) {
        case 2: {
            params.push_back(GetRegister(instr.gpr8));
            params.push_back(GetRegister(instr.gpr8.Value() + 1));
            break;
        }
        case 3: {
            params.push_back(GetRegister(instr.gpr8));
            params.push_back(GetRegister(instr.gpr8.Value() + 1));
            params.push_back(GetRegister(instr.gpr8.Value() + 2));
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unhandled coordinates number {}", static_cast<u32>(num_coordinates));
            params.push_back(GetRegister(instr.gpr8));
            params.push_back(GetRegister(instr.gpr8.Value() + 1));
            num_coordinates = 2;
            texture_type = Tegra::Shader::TextureType::Texture2D;
        }
        params.push_back(Immediate(static_cast<u32>(instr.tld4.component)));

        const auto& sampler = GetSampler(instr.sampler, texture_type, false, depth_compare);
        MetaTexture meta{sampler, num_coordinates};

        const Node texture =
            Operation(OperationCode::F4TextureGather, std::move(meta), std::move(params));

        if (depth_compare) {
            SetRegister(bb, instr.gpr0, texture);
        } else {
            MetaComponents meta;
            std::array<Node, 4> dest;

            std::size_t dest_elem = 0;
            for (std::size_t elem = 0; elem < 4; ++elem) {
                if (!instr.tex.IsComponentEnabled(elem)) {
                    // Skip disabled components
                    continue;
                }
                meta.components_map[dest_elem] = static_cast<u32>(elem);
                dest[dest_elem] = GetRegister(instr.gpr0.Value() + dest_elem);

                ++dest_elem;
            }
            std::generate(dest.begin() + dest_elem, dest.end(), [&]() { return GetRegister(RZ); });

            bb.push_back(Operation(OperationCode::AssignComposite, std::move(meta), texture,
                                   dest[0], dest[1], dest[2], dest[3]));
        }
        break;
    }
    case OpCode::Id::TLD4S: {
        UNIMPLEMENTED_IF_MSG(instr.tld4s.UsesMiscMode(TextureMiscMode::AOFFI),
                             "AOFFI is not implemented");

        if (instr.tld4s.UsesMiscMode(TextureMiscMode::NODEP)) {
            LOG_WARNING(HW_GPU, "TLD4S.NODEP implementation is incomplete");
        }

        const bool depth_compare = instr.tld4s.UsesMiscMode(TextureMiscMode::DC);
        const Node op_a = GetRegister(instr.gpr8);
        const Node op_b = GetRegister(instr.gpr20);

        std::vector<Node> params;

        // TODO(Subv): Figure out how the sampler type is encoded in the TLD4S instruction.
        if (depth_compare) {
            // Note: TLD4S coordinate encoding works just like TEXS's
            const Node op_y = GetRegister(instr.gpr8.Value() + 1);
            params.push_back(op_a);
            params.push_back(op_y);
            params.push_back(op_b);
        } else {
            params.push_back(op_a);
            params.push_back(op_b);
        }
        const auto num_coords = static_cast<u32>(params.size());
        params.push_back(Immediate(static_cast<u32>(instr.tld4s.component)));

        const auto& sampler =
            GetSampler(instr.sampler, TextureType::Texture2D, false, depth_compare);
        MetaTexture meta{sampler, num_coords};

        WriteTexsInstructionFloat(
            bb, instr, Operation(OperationCode::F4TextureGather, meta, std::move(params)));
        break;
    }
    case OpCode::Id::TXQ: {
        if (instr.txq.UsesMiscMode(TextureMiscMode::NODEP)) {
            LOG_WARNING(HW_GPU, "TXQ.NODEP implementation is incomplete");
        }

        // TODO: The new commits on the texture refactor, change the way samplers work.
        // Sadly, not all texture instructions specify the type of texture their sampler
        // uses. This must be fixed at a later instance.
        const auto& sampler =
            GetSampler(instr.sampler, Tegra::Shader::TextureType::Texture2D, false, false);

        switch (instr.txq.query_type) {
        case Tegra::Shader::TextureQueryType::Dimension: {
            MetaTexture meta_texture{sampler};
            const MetaComponents meta_components{{0, 1, 2, 3}};

            const Node texture = Operation(OperationCode::F4TextureQueryDimensions, meta_texture,
                                           GetRegister(instr.gpr8));
            std::array<Node, 4> dest;
            for (std::size_t i = 0; i < dest.size(); ++i) {
                dest[i] = GetRegister(instr.gpr0.Value() + i);
            }

            bb.push_back(Operation(OperationCode::AssignComposite, meta_components, texture,
                                   dest[0], dest[1], dest[2], dest[3]));
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unhandled texture query type: {}",
                              static_cast<u32>(instr.txq.query_type.Value()));
        }
        break;
    }
    case OpCode::Id::TMML: {
        UNIMPLEMENTED_IF_MSG(instr.tmml.UsesMiscMode(Tegra::Shader::TextureMiscMode::NDV),
                             "NDV is not implemented");

        if (instr.tmml.UsesMiscMode(TextureMiscMode::NODEP)) {
            LOG_WARNING(HW_GPU, "TMML.NODEP implementation is incomplete");
        }

        auto texture_type = instr.tmml.texture_type.Value();
        const bool is_array = instr.tmml.array != 0;
        const auto& sampler = GetSampler(instr.sampler, texture_type, is_array, false);

        std::vector<Node> coords;

        // TODO: Add coordinates for different samplers once other texture types are implemented.
        switch (texture_type) {
        case TextureType::Texture1D:
            coords.push_back(GetRegister(instr.gpr8));
            break;
        case TextureType::Texture2D:
            coords.push_back(GetRegister(instr.gpr8.Value() + 0));
            coords.push_back(GetRegister(instr.gpr8.Value() + 1));
            break;
        default:
            UNIMPLEMENTED_MSG("Unhandled texture type {}", static_cast<u32>(texture_type));

            // Fallback to interpreting as a 2D texture for now
            coords.push_back(GetRegister(instr.gpr8.Value() + 0));
            coords.push_back(GetRegister(instr.gpr8.Value() + 1));
            texture_type = TextureType::Texture2D;
        }

        MetaTexture meta_texture{sampler, static_cast<u32>(coords.size())};
        const Node texture =
            Operation(OperationCode::F4TextureQueryLod, meta_texture, std::move(coords));

        const MetaComponents meta_composite{{0, 1, 2, 3}};
        bb.push_back(Operation(OperationCode::AssignComposite, meta_composite, texture,
                               GetRegister(instr.gpr0), GetRegister(instr.gpr0.Value() + 1),
                               GetRegister(RZ), GetRegister(RZ)));
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled memory instruction: {}", opcode->get().GetName());
    }

    return pc;
}

const Sampler& ShaderIR::GetSampler(const Tegra::Shader::Sampler& sampler, TextureType type,
                                    bool is_array, bool is_shadow) {
    const auto offset = static_cast<std::size_t>(sampler.index.Value());

    // If this sampler has already been used, return the existing mapping.
    const auto itr =
        std::find_if(used_samplers.begin(), used_samplers.end(),
                     [&](const Sampler& entry) { return entry.GetOffset() == offset; });
    if (itr != used_samplers.end()) {
        ASSERT(itr->GetType() == type && itr->IsArray() == is_array &&
               itr->IsShadow() == is_shadow);
        return *itr;
    }

    // Otherwise create a new mapping for this sampler
    const std::size_t next_index = used_samplers.size();
    const Sampler entry{offset, next_index, type, is_array, is_shadow};
    return *used_samplers.emplace(entry).first;
}

void ShaderIR::WriteTexsInstructionFloat(BasicBlock& bb, Tegra::Shader::Instruction instr,
                                         Node texture) {
    // TEXS has two destination registers and a swizzle. The first two elements in the swizzle
    // go into gpr0+0 and gpr0+1, and the rest goes into gpr28+0 and gpr28+1

    MetaComponents meta;
    std::array<Node, 4> dest;

    std::size_t written_components = 0;
    for (u32 component = 0; component < 4; ++component) {
        if (!instr.texs.IsComponentEnabled(component)) {
            continue;
        }
        meta.components_map[written_components] = static_cast<u32>(component);

        if (written_components < 2) {
            // Write the first two swizzle components to gpr0 and gpr0+1
            dest[written_components] = GetRegister(instr.gpr0.Value() + written_components % 2);
        } else {
            ASSERT(instr.texs.HasTwoDestinations());
            // Write the rest of the swizzle components to gpr28 and gpr28+1
            dest[written_components] = GetRegister(instr.gpr28.Value() + written_components % 2);
        }

        ++written_components;
    }

    std::generate(dest.begin() + written_components, dest.end(), [&]() { return GetRegister(RZ); });

    bb.push_back(Operation(OperationCode::AssignComposite, meta, texture, dest[0], dest[1], dest[2],
                           dest[3]));
}

Node ShaderIR::GetTextureCode(Instruction instr, TextureType texture_type,
                              TextureProcessMode process_mode, bool depth_compare, bool is_array,
                              std::size_t array_offset, std::size_t bias_offset,
                              std::vector<Node>&& coords) {
    UNIMPLEMENTED_IF_MSG(
        (texture_type == TextureType::Texture3D && (is_array || depth_compare)) ||
            (texture_type == TextureType::TextureCube && is_array && depth_compare),
        "This method is not supported.");

    const auto& sampler = GetSampler(instr.sampler, texture_type, is_array, depth_compare);

    const bool lod_needed = process_mode == TextureProcessMode::LZ ||
                            process_mode == TextureProcessMode::LL ||
                            process_mode == TextureProcessMode::LLA;

    // LOD selection (either via bias or explicit textureLod) not supported in GL for
    // sampler2DArrayShadow and samplerCubeArrayShadow.
    const bool gl_lod_supported =
        !((texture_type == Tegra::Shader::TextureType::Texture2D && is_array && depth_compare) ||
          (texture_type == Tegra::Shader::TextureType::TextureCube && is_array && depth_compare));

    const OperationCode read_method =
        lod_needed && gl_lod_supported ? OperationCode::F4TextureLod : OperationCode::F4Texture;

    UNIMPLEMENTED_IF(process_mode != TextureProcessMode::None && !gl_lod_supported);

    std::optional<u32> array_offset_value;
    if (is_array)
        array_offset_value = static_cast<u32>(array_offset);
    MetaTexture meta{sampler, static_cast<u32>(coords.size()), array_offset_value};
    std::vector<Node> params = std::move(coords);

    if (process_mode != TextureProcessMode::None && gl_lod_supported) {
        if (process_mode == TextureProcessMode::LZ) {
            params.push_back(Immediate(0.0f));
        } else {
            // If present, lod or bias are always stored in the register indexed by the gpr20 field
            // with an offset depending on the usage of the other registers
            params.push_back(GetRegister(instr.gpr20.Value() + bias_offset));
        }
    }

    return Operation(read_method, meta, std::move(params));
}

Node ShaderIR::GetTexCode(Instruction instr, TextureType texture_type,
                          TextureProcessMode process_mode, bool depth_compare, bool is_array) {
    const bool lod_bias_enabled =
        (process_mode != TextureProcessMode::None && process_mode != TextureProcessMode::LZ);

    const auto [coord_count, total_coord_count] = ValidateAndGetCoordinateElement(
        texture_type, depth_compare, is_array, lod_bias_enabled, 4, 5);
    // If enabled arrays index is always stored in the gpr8 field
    const u64 array_register = instr.gpr8.Value();
    // First coordinate index is the gpr8 or gpr8 + 1 when arrays are used
    const u64 coord_register = array_register + (is_array ? 1 : 0);

    std::vector<Node> coords;
    for (std::size_t i = 0; i < coord_count; ++i) {
        coords.push_back(GetRegister(coord_register + i));
    }
    // 1D.DC in opengl the 2nd component is ignored.
    if (depth_compare && !is_array && texture_type == TextureType::Texture1D) {
        coords.push_back(Immediate(0.0f));
    }
    std::size_t array_offset{};
    if (is_array) {
        array_offset = coords.size();
        coords.push_back(GetRegister(array_register));
    }
    if (depth_compare) {
        // Depth is always stored in the register signaled by gpr20
        // or in the next register if lod or bias are used
        const u64 depth_register = instr.gpr20.Value() + (lod_bias_enabled ? 1 : 0);
        coords.push_back(GetRegister(depth_register));
    }
    // Fill ignored coordinates
    while (coords.size() < total_coord_count) {
        coords.push_back(Immediate(0));
    }

    return GetTextureCode(instr, texture_type, process_mode, depth_compare, is_array, array_offset,
                          0, std::move(coords));
}

Node ShaderIR::GetTexsCode(Instruction instr, TextureType texture_type,
                           TextureProcessMode process_mode, bool depth_compare, bool is_array) {
    const bool lod_bias_enabled =
        (process_mode != TextureProcessMode::None && process_mode != TextureProcessMode::LZ);

    const auto [coord_count, total_coord_count] = ValidateAndGetCoordinateElement(
        texture_type, depth_compare, is_array, lod_bias_enabled, 4, 4);
    // If enabled arrays index is always stored in the gpr8 field
    const u64 array_register = instr.gpr8.Value();
    // First coordinate index is stored in gpr8 field or (gpr8 + 1) when arrays are used
    const u64 coord_register = array_register + (is_array ? 1 : 0);
    const u64 last_coord_register =
        (is_array || !(lod_bias_enabled || depth_compare) || (coord_count > 2))
            ? static_cast<u64>(instr.gpr20.Value())
            : coord_register + 1;

    std::vector<Node> coords;
    for (std::size_t i = 0; i < coord_count; ++i) {
        const bool last = (i == (coord_count - 1)) && (coord_count > 1);
        coords.push_back(GetRegister(last ? last_coord_register : coord_register + i));
    }

    std::size_t array_offset{};
    if (is_array) {
        array_offset = coords.size();
        coords.push_back(GetRegister(array_register));
    }
    if (depth_compare) {
        // Depth is always stored in the register signaled by gpr20
        // or in the next register if lod or bias are used
        const u64 depth_register = instr.gpr20.Value() + (lod_bias_enabled ? 1 : 0);
        coords.push_back(GetRegister(depth_register));
    }
    // Fill ignored coordinates
    while (coords.size() < total_coord_count) {
        coords.push_back(Immediate(0));
    }

    return GetTextureCode(instr, texture_type, process_mode, depth_compare, is_array, array_offset,
                          (coord_count > 2 ? 1 : 0), std::move(coords));
}

std::tuple<std::size_t, std::size_t> ShaderIR::ValidateAndGetCoordinateElement(
    TextureType texture_type, bool depth_compare, bool is_array, bool lod_bias_enabled,
    std::size_t max_coords, std::size_t max_inputs) {

    const std::size_t coord_count = GetCoordCount(texture_type);

    std::size_t total_coord_count = coord_count + (is_array ? 1 : 0) + (depth_compare ? 1 : 0);
    const std::size_t total_reg_count = total_coord_count + (lod_bias_enabled ? 1 : 0);
    if (total_coord_count > max_coords || total_reg_count > max_inputs) {
        UNIMPLEMENTED_MSG("Unsupported Texture operation");
        total_coord_count = std::min(total_coord_count, max_coords);
    }
    // 1D.DC OpenGL is using a vec3 but 2nd component is ignored later.
    total_coord_count +=
        (depth_compare && !is_array && texture_type == TextureType::Texture1D) ? 1 : 0;

    return {coord_count, total_coord_count};
}

} // namespace VideoCommon::Shader