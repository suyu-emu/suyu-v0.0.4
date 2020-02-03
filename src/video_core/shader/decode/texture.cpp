// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>
#include <fmt/format.h>

#include "common/assert.h"
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/node_helper.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

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

u32 ShaderIR::DecodeTexture(NodeBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);
    bool is_bindless = false;
    switch (opcode->get().GetId()) {
    case OpCode::Id::TEX: {
        const TextureType texture_type{instr.tex.texture_type};
        const bool is_array = instr.tex.array != 0;
        const bool is_aoffi = instr.tex.UsesMiscMode(TextureMiscMode::AOFFI);
        const bool depth_compare = instr.tex.UsesMiscMode(TextureMiscMode::DC);
        const auto process_mode = instr.tex.GetTextureProcessMode();
        WriteTexInstructionFloat(
            bb, instr,
            GetTexCode(instr, texture_type, process_mode, depth_compare, is_array, is_aoffi, {}));
        break;
    }
    case OpCode::Id::TEX_B: {
        UNIMPLEMENTED_IF_MSG(instr.tex.UsesMiscMode(TextureMiscMode::AOFFI),
                             "AOFFI is not implemented");

        const TextureType texture_type{instr.tex_b.texture_type};
        const bool is_array = instr.tex_b.array != 0;
        const bool is_aoffi = instr.tex.UsesMiscMode(TextureMiscMode::AOFFI);
        const bool depth_compare = instr.tex_b.UsesMiscMode(TextureMiscMode::DC);
        const auto process_mode = instr.tex_b.GetTextureProcessMode();
        WriteTexInstructionFloat(bb, instr,
                                 GetTexCode(instr, texture_type, process_mode, depth_compare,
                                            is_array, is_aoffi, {instr.gpr20}));
        break;
    }
    case OpCode::Id::TEXS: {
        const TextureType texture_type{instr.texs.GetTextureType()};
        const bool is_array{instr.texs.IsArrayTexture()};
        const bool depth_compare = instr.texs.UsesMiscMode(TextureMiscMode::DC);
        const auto process_mode = instr.texs.GetTextureProcessMode();

        const Node4 components =
            GetTexsCode(instr, texture_type, process_mode, depth_compare, is_array);

        if (instr.texs.fp32_flag) {
            WriteTexsInstructionFloat(bb, instr, components);
        } else {
            WriteTexsInstructionHalfFloat(bb, instr, components);
        }
        break;
    }
    case OpCode::Id::TLD4_B: {
        is_bindless = true;
        [[fallthrough]];
    }
    case OpCode::Id::TLD4: {
        UNIMPLEMENTED_IF_MSG(instr.tld4.UsesMiscMode(TextureMiscMode::NDV),
                             "NDV is not implemented");
        const auto texture_type = instr.tld4.texture_type.Value();
        const bool depth_compare = is_bindless ? instr.tld4_b.UsesMiscMode(TextureMiscMode::DC)
                                               : instr.tld4.UsesMiscMode(TextureMiscMode::DC);
        const bool is_array = instr.tld4.array != 0;
        const bool is_aoffi = is_bindless ? instr.tld4_b.UsesMiscMode(TextureMiscMode::AOFFI)
                                          : instr.tld4.UsesMiscMode(TextureMiscMode::AOFFI);
        const bool is_ptp = is_bindless ? instr.tld4_b.UsesMiscMode(TextureMiscMode::PTP)
                                        : instr.tld4.UsesMiscMode(TextureMiscMode::PTP);
        WriteTexInstructionFloat(bb, instr,
                                 GetTld4Code(instr, texture_type, depth_compare, is_array, is_aoffi,
                                             is_ptp, is_bindless));
        break;
    }
    case OpCode::Id::TLD4S: {
        constexpr std::size_t num_coords = 2;
        const bool is_aoffi = instr.tld4s.UsesMiscMode(TextureMiscMode::AOFFI);
        const bool is_depth_compare = instr.tld4s.UsesMiscMode(TextureMiscMode::DC);
        const Node op_a = GetRegister(instr.gpr8);
        const Node op_b = GetRegister(instr.gpr20);

        // TODO(Subv): Figure out how the sampler type is encoded in the TLD4S instruction.
        std::vector<Node> coords;
        std::vector<Node> aoffi;
        Node depth_compare;
        if (is_depth_compare) {
            // Note: TLD4S coordinate encoding works just like TEXS's
            const Node op_y = GetRegister(instr.gpr8.Value() + 1);
            coords.push_back(op_a);
            coords.push_back(op_y);
            if (is_aoffi) {
                aoffi = GetAoffiCoordinates(op_b, num_coords, true);
                depth_compare = GetRegister(instr.gpr20.Value() + 1);
            } else {
                depth_compare = op_b;
            }
        } else {
            // There's no depth compare
            coords.push_back(op_a);
            if (is_aoffi) {
                coords.push_back(GetRegister(instr.gpr8.Value() + 1));
                aoffi = GetAoffiCoordinates(op_b, num_coords, true);
            } else {
                coords.push_back(op_b);
            }
        }
        const Node component = Immediate(static_cast<u32>(instr.tld4s.component));

        const SamplerInfo info{TextureType::Texture2D, false, is_depth_compare};
        const Sampler& sampler = *GetSampler(instr.sampler, info);

        Node4 values;
        for (u32 element = 0; element < values.size(); ++element) {
            auto coords_copy = coords;
            MetaTexture meta{sampler, {}, depth_compare, aoffi,   {}, {},
                             {},      {}, component,     element, {}};
            values[element] = Operation(OperationCode::TextureGather, meta, std::move(coords_copy));
        }

        if (instr.tld4s.fp16_flag) {
            WriteTexsInstructionHalfFloat(bb, instr, values, true);
        } else {
            WriteTexsInstructionFloat(bb, instr, values, true);
        }
        break;
    }
    case OpCode::Id::TXD_B:
        is_bindless = true;
        [[fallthrough]];
    case OpCode::Id::TXD: {
        UNIMPLEMENTED_IF_MSG(instr.txd.UsesMiscMode(TextureMiscMode::AOFFI),
                             "AOFFI is not implemented");

        const bool is_array = instr.txd.is_array != 0;
        u64 base_reg = instr.gpr8.Value();
        const auto derivate_reg = instr.gpr20.Value();
        const auto texture_type = instr.txd.texture_type.Value();
        const auto coord_count = GetCoordCount(texture_type);
        Node index_var{};
        const Sampler* sampler =
            is_bindless ? GetBindlessSampler(base_reg, index_var, {{texture_type, is_array, false}})
                        : GetSampler(instr.sampler, {{texture_type, is_array, false}});
        Node4 values;
        if (sampler == nullptr) {
            for (u32 element = 0; element < values.size(); ++element) {
                values[element] = Immediate(0);
            }
            WriteTexInstructionFloat(bb, instr, values);
            break;
        }

        if (is_bindless) {
            base_reg++;
        }

        std::vector<Node> coords;
        std::vector<Node> derivates;
        for (std::size_t i = 0; i < coord_count; ++i) {
            coords.push_back(GetRegister(base_reg + i));
            const std::size_t derivate = i * 2;
            derivates.push_back(GetRegister(derivate_reg + derivate));
            derivates.push_back(GetRegister(derivate_reg + derivate + 1));
        }

        Node array_node = {};
        if (is_array) {
            const Node info_reg = GetRegister(base_reg + coord_count);
            array_node = BitfieldExtract(info_reg, 0, 16);
        }

        for (u32 element = 0; element < values.size(); ++element) {
            MetaTexture meta{*sampler, array_node, {}, {},      {},       derivates,
                             {},       {},         {}, element, index_var};
            values[element] = Operation(OperationCode::TextureGradient, std::move(meta), coords);
        }

        WriteTexInstructionFloat(bb, instr, values);

        break;
    }
    case OpCode::Id::TXQ_B:
        is_bindless = true;
        [[fallthrough]];
    case OpCode::Id::TXQ: {
        // TODO: The new commits on the texture refactor, change the way samplers work.
        // Sadly, not all texture instructions specify the type of texture their sampler
        // uses. This must be fixed at a later instance.
        Node index_var{};
        const Sampler* sampler =
            is_bindless ? GetBindlessSampler(instr.gpr8, index_var) : GetSampler(instr.sampler);

        if (sampler == nullptr) {
            u32 indexer = 0;
            for (u32 element = 0; element < 4; ++element) {
                if (!instr.txq.IsComponentEnabled(element)) {
                    continue;
                }
                const Node value = Immediate(0);
                SetTemporary(bb, indexer++, value);
            }
            for (u32 i = 0; i < indexer; ++i) {
                SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
            }
            break;
        }

        u32 indexer = 0;
        switch (instr.txq.query_type) {
        case Tegra::Shader::TextureQueryType::Dimension: {
            for (u32 element = 0; element < 4; ++element) {
                if (!instr.txq.IsComponentEnabled(element)) {
                    continue;
                }
                MetaTexture meta{*sampler, {}, {}, {}, {}, {}, {}, {}, {}, element, index_var};
                const Node value =
                    Operation(OperationCode::TextureQueryDimensions, meta,
                              GetRegister(instr.gpr8.Value() + (is_bindless ? 1 : 0)));
                SetTemporary(bb, indexer++, value);
            }
            for (u32 i = 0; i < indexer; ++i) {
                SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
            }
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unhandled texture query type: {}",
                              static_cast<u32>(instr.txq.query_type.Value()));
        }
        break;
    }
    case OpCode::Id::TMML_B:
        is_bindless = true;
        [[fallthrough]];
    case OpCode::Id::TMML: {
        UNIMPLEMENTED_IF_MSG(instr.tmml.UsesMiscMode(Tegra::Shader::TextureMiscMode::NDV),
                             "NDV is not implemented");

        auto texture_type = instr.tmml.texture_type.Value();
        const bool is_array = instr.tmml.array != 0;
        Node index_var{};
        const Sampler* sampler =
            is_bindless ? GetBindlessSampler(instr.gpr20, index_var) : GetSampler(instr.sampler);

        if (sampler == nullptr) {
            u32 indexer = 0;
            for (u32 element = 0; element < 2; ++element) {
                if (!instr.tmml.IsComponentEnabled(element)) {
                    continue;
                }
                const Node value = Immediate(0);
                SetTemporary(bb, indexer++, value);
            }
            for (u32 i = 0; i < indexer; ++i) {
                SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
            }
            break;
        }

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
        u32 indexer = 0;
        for (u32 element = 0; element < 2; ++element) {
            if (!instr.tmml.IsComponentEnabled(element)) {
                continue;
            }
            auto params = coords;
            MetaTexture meta{*sampler, {}, {}, {}, {}, {}, {}, {}, {}, element, index_var};
            const Node value = Operation(OperationCode::TextureQueryLod, meta, std::move(params));
            SetTemporary(bb, indexer++, value);
        }
        for (u32 i = 0; i < indexer; ++i) {
            SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
        }
        break;
    }
    case OpCode::Id::TLD: {
        UNIMPLEMENTED_IF_MSG(instr.tld.aoffi, "AOFFI is not implemented");
        UNIMPLEMENTED_IF_MSG(instr.tld.ms, "MS is not implemented");
        UNIMPLEMENTED_IF_MSG(instr.tld.cl, "CL is not implemented");

        WriteTexInstructionFloat(bb, instr, GetTldCode(instr));
        break;
    }
    case OpCode::Id::TLDS: {
        const TextureType texture_type{instr.tlds.GetTextureType()};
        const bool is_array{instr.tlds.IsArrayTexture()};

        UNIMPLEMENTED_IF_MSG(instr.tlds.UsesMiscMode(TextureMiscMode::AOFFI),
                             "AOFFI is not implemented");
        UNIMPLEMENTED_IF_MSG(instr.tlds.UsesMiscMode(TextureMiscMode::MZ), "MZ is not implemented");

        const Node4 components = GetTldsCode(instr, texture_type, is_array);

        if (instr.tlds.fp32_flag) {
            WriteTexsInstructionFloat(bb, instr, components);
        } else {
            WriteTexsInstructionHalfFloat(bb, instr, components);
        }
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled memory instruction: {}", opcode->get().GetName());
    }

    return pc;
}

ShaderIR::SamplerInfo ShaderIR::GetSamplerInfo(std::optional<SamplerInfo> sampler_info, u32 offset,
                                               std::optional<u32> buffer) {
    if (sampler_info) {
        return *sampler_info;
    }
    const auto sampler =
        buffer ? locker.ObtainBindlessSampler(*buffer, offset) : locker.ObtainBoundSampler(offset);
    if (!sampler) {
        LOG_WARNING(HW_GPU, "Unknown sampler info");
        return SamplerInfo{TextureType::Texture2D, false, false, false};
    }
    return SamplerInfo{sampler->texture_type, sampler->is_array != 0, sampler->is_shadow != 0,
                       sampler->is_buffer != 0};
}

const Sampler* ShaderIR::GetSampler(const Tegra::Shader::Sampler& sampler,
                                    std::optional<SamplerInfo> sampler_info) {
    const auto offset = static_cast<u32>(sampler.index.Value());
    const auto info = GetSamplerInfo(sampler_info, offset);

    // If this sampler has already been used, return the existing mapping.
    const auto it =
        std::find_if(used_samplers.begin(), used_samplers.end(),
                     [offset](const Sampler& entry) { return entry.GetOffset() == offset; });
    if (it != used_samplers.end()) {
        ASSERT(!it->IsBindless() && it->GetType() == info.type && it->IsArray() == info.is_array &&
               it->IsShadow() == info.is_shadow && it->IsBuffer() == info.is_buffer);
        return &*it;
    }

    // Otherwise create a new mapping for this sampler
    const auto next_index = static_cast<u32>(used_samplers.size());
    return &used_samplers.emplace_back(next_index, offset, info.type, info.is_array, info.is_shadow,
                                       info.is_buffer, false);
}

const Sampler* ShaderIR::GetBindlessSampler(Tegra::Shader::Register reg, Node& index_var,
                                            std::optional<SamplerInfo> sampler_info) {
    const Node sampler_register = GetRegister(reg);
    const auto [base_node, tracked_sampler_info] =
        TrackBindlessSampler(sampler_register, global_code, static_cast<s64>(global_code.size()));
    ASSERT(base_node != nullptr);
    if (base_node == nullptr) {
        return nullptr;
    }

    if (const auto bindless_sampler_info =
            std::get_if<BindlessSamplerNode>(&*tracked_sampler_info)) {
        const u32 buffer = bindless_sampler_info->GetIndex();
        const u32 offset = bindless_sampler_info->GetOffset();
        const auto info = GetSamplerInfo(sampler_info, offset, buffer);

        // If this sampler has already been used, return the existing mapping.
        const auto it =
            std::find_if(used_samplers.begin(), used_samplers.end(),
                         [buffer = buffer, offset = offset](const Sampler& entry) {
                             return entry.GetBuffer() == buffer && entry.GetOffset() == offset;
                         });
        if (it != used_samplers.end()) {
            ASSERT(it->IsBindless() && it->GetType() == info.type &&
                   it->IsArray() == info.is_array && it->IsShadow() == info.is_shadow);
            return &*it;
        }

        // Otherwise create a new mapping for this sampler
        const auto next_index = static_cast<u32>(used_samplers.size());
        return &used_samplers.emplace_back(next_index, offset, buffer, info.type, info.is_array,
                                           info.is_shadow, info.is_buffer, false);
    } else if (const auto array_sampler_info =
                   std::get_if<ArraySamplerNode>(&*tracked_sampler_info)) {
        const u32 base_offset = array_sampler_info->GetBaseOffset() / 4;
        index_var = GetCustomVariable(array_sampler_info->GetIndexVar());
        const auto info = GetSamplerInfo(sampler_info, base_offset);

        // If this sampler has already been used, return the existing mapping.
        const auto it = std::find_if(
            used_samplers.begin(), used_samplers.end(),
            [base_offset](const Sampler& entry) { return entry.GetOffset() == base_offset; });
        if (it != used_samplers.end()) {
            ASSERT(!it->IsBindless() && it->GetType() == info.type &&
                   it->IsArray() == info.is_array && it->IsShadow() == info.is_shadow &&
                   it->IsBuffer() == info.is_buffer && it->IsIndexed());
            return &*it;
        }

        uses_indexed_samplers = true;
        // Otherwise create a new mapping for this sampler
        const auto next_index = static_cast<u32>(used_samplers.size());
        return &used_samplers.emplace_back(next_index, base_offset, info.type, info.is_array,
                                           info.is_shadow, info.is_buffer, true);
    }
    return nullptr;
}

void ShaderIR::WriteTexInstructionFloat(NodeBlock& bb, Instruction instr, const Node4& components) {
    u32 dest_elem = 0;
    for (u32 elem = 0; elem < 4; ++elem) {
        if (!instr.tex.IsComponentEnabled(elem)) {
            // Skip disabled components
            continue;
        }
        SetTemporary(bb, dest_elem++, components[elem]);
    }
    // After writing values in temporals, move them to the real registers
    for (u32 i = 0; i < dest_elem; ++i) {
        SetRegister(bb, instr.gpr0.Value() + i, GetTemporary(i));
    }
}

void ShaderIR::WriteTexsInstructionFloat(NodeBlock& bb, Instruction instr, const Node4& components,
                                         bool ignore_mask) {
    // TEXS has two destination registers and a swizzle. The first two elements in the swizzle
    // go into gpr0+0 and gpr0+1, and the rest goes into gpr28+0 and gpr28+1

    u32 dest_elem = 0;
    for (u32 component = 0; component < 4; ++component) {
        if (!instr.texs.IsComponentEnabled(component) && !ignore_mask)
            continue;
        SetTemporary(bb, dest_elem++, components[component]);
    }

    for (u32 i = 0; i < dest_elem; ++i) {
        if (i < 2) {
            // Write the first two swizzle components to gpr0 and gpr0+1
            SetRegister(bb, instr.gpr0.Value() + i % 2, GetTemporary(i));
        } else {
            ASSERT(instr.texs.HasTwoDestinations());
            // Write the rest of the swizzle components to gpr28 and gpr28+1
            SetRegister(bb, instr.gpr28.Value() + i % 2, GetTemporary(i));
        }
    }
}

void ShaderIR::WriteTexsInstructionHalfFloat(NodeBlock& bb, Instruction instr,
                                             const Node4& components, bool ignore_mask) {
    // TEXS.F16 destionation registers are packed in two registers in pairs (just like any half
    // float instruction).

    Node4 values;
    u32 dest_elem = 0;
    for (u32 component = 0; component < 4; ++component) {
        if (!instr.texs.IsComponentEnabled(component) && !ignore_mask)
            continue;
        values[dest_elem++] = components[component];
    }
    if (dest_elem == 0)
        return;

    std::generate(values.begin() + dest_elem, values.end(), [&]() { return Immediate(0); });

    const Node first_value = Operation(OperationCode::HPack2, values[0], values[1]);
    if (dest_elem <= 2) {
        SetRegister(bb, instr.gpr0, first_value);
        return;
    }

    SetTemporary(bb, 0, first_value);
    SetTemporary(bb, 1, Operation(OperationCode::HPack2, values[2], values[3]));

    SetRegister(bb, instr.gpr0, GetTemporary(0));
    SetRegister(bb, instr.gpr28, GetTemporary(1));
}

Node4 ShaderIR::GetTextureCode(Instruction instr, TextureType texture_type,
                               TextureProcessMode process_mode, std::vector<Node> coords,
                               Node array, Node depth_compare, u32 bias_offset,
                               std::vector<Node> aoffi,
                               std::optional<Tegra::Shader::Register> bindless_reg) {
    const auto is_array = static_cast<bool>(array);
    const auto is_shadow = static_cast<bool>(depth_compare);
    const bool is_bindless = bindless_reg.has_value();

    UNIMPLEMENTED_IF_MSG((texture_type == TextureType::Texture3D && (is_array || is_shadow)) ||
                             (texture_type == TextureType::TextureCube && is_array && is_shadow),
                         "This method is not supported.");

    const SamplerInfo info{texture_type, is_array, is_shadow, false};
    Node index_var{};
    const Sampler* sampler = is_bindless ? GetBindlessSampler(*bindless_reg, index_var, info)
                                         : GetSampler(instr.sampler, info);
    Node4 values;
    if (sampler == nullptr) {
        for (u32 element = 0; element < values.size(); ++element) {
            values[element] = Immediate(0);
        }
        return values;
    }

    const bool lod_needed = process_mode == TextureProcessMode::LZ ||
                            process_mode == TextureProcessMode::LL ||
                            process_mode == TextureProcessMode::LLA;

    // LOD selection (either via bias or explicit textureLod) not supported in GL for
    // sampler2DArrayShadow and samplerCubeArrayShadow.
    const bool gl_lod_supported =
        !((texture_type == Tegra::Shader::TextureType::Texture2D && is_array && is_shadow) ||
          (texture_type == Tegra::Shader::TextureType::TextureCube && is_array && is_shadow));

    const OperationCode read_method =
        (lod_needed && gl_lod_supported) ? OperationCode::TextureLod : OperationCode::Texture;

    UNIMPLEMENTED_IF(process_mode != TextureProcessMode::None && !gl_lod_supported);

    Node bias;
    Node lod;
    if (process_mode != TextureProcessMode::None && gl_lod_supported) {
        switch (process_mode) {
        case TextureProcessMode::LZ:
            lod = Immediate(0.0f);
            break;
        case TextureProcessMode::LB:
            // If present, lod or bias are always stored in the register
            // indexed by the gpr20 field with an offset depending on the
            // usage of the other registers
            bias = GetRegister(instr.gpr20.Value() + bias_offset);
            break;
        case TextureProcessMode::LL:
            lod = GetRegister(instr.gpr20.Value() + bias_offset);
            break;
        default:
            UNIMPLEMENTED_MSG("Unimplemented process mode={}", static_cast<u32>(process_mode));
            break;
        }
    }

    for (u32 element = 0; element < values.size(); ++element) {
        auto copy_coords = coords;
        MetaTexture meta{*sampler, array, depth_compare, aoffi,    {}, {}, bias,
                         lod,      {},    element,       index_var};
        values[element] = Operation(read_method, meta, std::move(copy_coords));
    }

    return values;
}

Node4 ShaderIR::GetTexCode(Instruction instr, TextureType texture_type,
                           TextureProcessMode process_mode, bool depth_compare, bool is_array,
                           bool is_aoffi, std::optional<Tegra::Shader::Register> bindless_reg) {
    const bool lod_bias_enabled{
        (process_mode != TextureProcessMode::None && process_mode != TextureProcessMode::LZ)};

    const bool is_bindless = bindless_reg.has_value();

    u64 parameter_register = instr.gpr20.Value();
    if (is_bindless) {
        ++parameter_register;
    }

    const u32 bias_lod_offset = (is_bindless ? 1 : 0);
    if (lod_bias_enabled) {
        ++parameter_register;
    }

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
    // 1D.DC in OpenGL the 2nd component is ignored.
    if (depth_compare && !is_array && texture_type == TextureType::Texture1D) {
        coords.push_back(Immediate(0.0f));
    }

    const Node array = is_array ? GetRegister(array_register) : nullptr;

    std::vector<Node> aoffi;
    if (is_aoffi) {
        aoffi = GetAoffiCoordinates(GetRegister(parameter_register++), coord_count, false);
    }

    Node dc;
    if (depth_compare) {
        // Depth is always stored in the register signaled by gpr20 or in the next register if lod
        // or bias are used
        dc = GetRegister(parameter_register++);
    }

    return GetTextureCode(instr, texture_type, process_mode, coords, array, dc, bias_lod_offset,
                          aoffi, bindless_reg);
}

Node4 ShaderIR::GetTexsCode(Instruction instr, TextureType texture_type,
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
    const u32 bias_offset = coord_count > 2 ? 1 : 0;

    std::vector<Node> coords;
    for (std::size_t i = 0; i < coord_count; ++i) {
        const bool last = (i == (coord_count - 1)) && (coord_count > 1);
        coords.push_back(GetRegister(last ? last_coord_register : coord_register + i));
    }

    const Node array = is_array ? GetRegister(array_register) : nullptr;

    Node dc;
    if (depth_compare) {
        // Depth is always stored in the register signaled by gpr20 or in the next register if lod
        // or bias are used
        const u64 depth_register = instr.gpr20.Value() + (lod_bias_enabled ? 1 : 0);
        dc = GetRegister(depth_register);
    }

    return GetTextureCode(instr, texture_type, process_mode, coords, array, dc, bias_offset, {},
                          {});
}

Node4 ShaderIR::GetTld4Code(Instruction instr, TextureType texture_type, bool depth_compare,
                            bool is_array, bool is_aoffi, bool is_ptp, bool is_bindless) {
    ASSERT_MSG(!(is_aoffi && is_ptp), "AOFFI and PTP can't be enabled at the same time");

    const std::size_t coord_count = GetCoordCount(texture_type);

    // If enabled arrays index is always stored in the gpr8 field
    const u64 array_register = instr.gpr8.Value();
    // First coordinate index is the gpr8 or gpr8 + 1 when arrays are used
    const u64 coord_register = array_register + (is_array ? 1 : 0);

    std::vector<Node> coords;
    for (std::size_t i = 0; i < coord_count; ++i) {
        coords.push_back(GetRegister(coord_register + i));
    }

    u64 parameter_register = instr.gpr20.Value();

    const SamplerInfo info{texture_type, is_array, depth_compare, false};
    Node index_var{};
    const Sampler* sampler = is_bindless ? GetBindlessSampler(parameter_register++, index_var, info)
                                         : GetSampler(instr.sampler, info);
    Node4 values;
    if (sampler == nullptr) {
        for (u32 element = 0; element < values.size(); ++element) {
            values[element] = Immediate(0);
        }
        return values;
    }

    std::vector<Node> aoffi, ptp;
    if (is_aoffi) {
        aoffi = GetAoffiCoordinates(GetRegister(parameter_register++), coord_count, true);
    } else if (is_ptp) {
        ptp = GetPtpCoordinates(
            {GetRegister(parameter_register++), GetRegister(parameter_register++)});
    }

    Node dc;
    if (depth_compare) {
        dc = GetRegister(parameter_register++);
    }

    const Node component = is_bindless ? Immediate(static_cast<u32>(instr.tld4_b.component))
                                       : Immediate(static_cast<u32>(instr.tld4.component));

    for (u32 element = 0; element < values.size(); ++element) {
        auto coords_copy = coords;
        MetaTexture meta{
            *sampler, GetRegister(array_register), dc, aoffi, ptp, {}, {}, {}, component, element,
            index_var};
        values[element] = Operation(OperationCode::TextureGather, meta, std::move(coords_copy));
    }

    return values;
}

Node4 ShaderIR::GetTldCode(Tegra::Shader::Instruction instr) {
    const auto texture_type{instr.tld.texture_type};
    const bool is_array{instr.tld.is_array};
    const bool lod_enabled{instr.tld.GetTextureProcessMode() == TextureProcessMode::LL};
    const std::size_t coord_count{GetCoordCount(texture_type)};

    u64 gpr8_cursor{instr.gpr8.Value()};
    const Node array_register{is_array ? GetRegister(gpr8_cursor++) : nullptr};

    std::vector<Node> coords;
    coords.reserve(coord_count);
    for (std::size_t i = 0; i < coord_count; ++i) {
        coords.push_back(GetRegister(gpr8_cursor++));
    }

    u64 gpr20_cursor{instr.gpr20.Value()};
    // const Node bindless_register{is_bindless ? GetRegister(gpr20_cursor++) : nullptr};
    const Node lod{lod_enabled ? GetRegister(gpr20_cursor++) : Immediate(0u)};
    // const Node aoffi_register{is_aoffi ? GetRegister(gpr20_cursor++) : nullptr};
    // const Node multisample{is_multisample ? GetRegister(gpr20_cursor++) : nullptr};

    const auto& sampler = *GetSampler(instr.sampler);

    Node4 values;
    for (u32 element = 0; element < values.size(); ++element) {
        auto coords_copy = coords;
        MetaTexture meta{sampler, array_register, {}, {}, {}, {}, {}, lod, {}, element, {}};
        values[element] = Operation(OperationCode::TexelFetch, meta, std::move(coords_copy));
    }

    return values;
}

Node4 ShaderIR::GetTldsCode(Instruction instr, TextureType texture_type, bool is_array) {
    const Sampler& sampler = *GetSampler(instr.sampler);

    const std::size_t type_coord_count = GetCoordCount(texture_type);
    const bool lod_enabled = instr.tlds.GetTextureProcessMode() == TextureProcessMode::LL;

    // If enabled arrays index is always stored in the gpr8 field
    const u64 array_register = instr.gpr8.Value();
    // if is array gpr20 is used
    const u64 coord_register = is_array ? instr.gpr20.Value() : instr.gpr8.Value();

    const u64 last_coord_register =
        ((type_coord_count > 2) || (type_coord_count == 2 && !lod_enabled)) && !is_array
            ? static_cast<u64>(instr.gpr20.Value())
            : coord_register + 1;

    std::vector<Node> coords;
    for (std::size_t i = 0; i < type_coord_count; ++i) {
        const bool last = (i == (type_coord_count - 1)) && (type_coord_count > 1);
        coords.push_back(GetRegister(last ? last_coord_register : coord_register + i));
    }

    const Node array = is_array ? GetRegister(array_register) : nullptr;
    // When lod is used always is in gpr20
    const Node lod = lod_enabled ? GetRegister(instr.gpr20) : Immediate(0);

    // Fill empty entries from the guest sampler
    const std::size_t entry_coord_count = GetCoordCount(sampler.GetType());
    if (type_coord_count != entry_coord_count) {
        LOG_WARNING(HW_GPU, "Bound and built texture types mismatch");

        // When the size is higher we insert zeroes
        for (std::size_t i = type_coord_count; i < entry_coord_count; ++i) {
            coords.push_back(GetRegister(Register::ZeroIndex));
        }

        // Then we ensure the size matches the number of entries (dropping unused values)
        coords.resize(entry_coord_count);
    }

    Node4 values;
    for (u32 element = 0; element < values.size(); ++element) {
        auto coords_copy = coords;
        MetaTexture meta{sampler, array, {}, {}, {}, {}, {}, lod, {}, element, {}};
        values[element] = Operation(OperationCode::TexelFetch, meta, std::move(coords_copy));
    }
    return values;
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

std::vector<Node> ShaderIR::GetAoffiCoordinates(Node aoffi_reg, std::size_t coord_count,
                                                bool is_tld4) {
    const std::array coord_offsets = is_tld4 ? std::array{0U, 8U, 16U} : std::array{0U, 4U, 8U};
    const u32 size = is_tld4 ? 6 : 4;
    const s32 wrap_value = is_tld4 ? 32 : 8;
    const s32 diff_value = is_tld4 ? 64 : 16;
    const u32 mask = (1U << size) - 1;

    std::vector<Node> aoffi;
    aoffi.reserve(coord_count);

    const auto aoffi_immediate{
        TrackImmediate(aoffi_reg, global_code, static_cast<s64>(global_code.size()))};
    if (!aoffi_immediate) {
        // Variable access, not supported on AMD.
        LOG_WARNING(HW_GPU,
                    "AOFFI constant folding failed, some hardware might have graphical issues");
        for (std::size_t coord = 0; coord < coord_count; ++coord) {
            const Node value = BitfieldExtract(aoffi_reg, coord_offsets[coord], size);
            const Node condition =
                Operation(OperationCode::LogicalIGreaterEqual, value, Immediate(wrap_value));
            const Node negative = Operation(OperationCode::IAdd, value, Immediate(-diff_value));
            aoffi.push_back(Operation(OperationCode::Select, condition, negative, value));
        }
        return aoffi;
    }

    for (std::size_t coord = 0; coord < coord_count; ++coord) {
        s32 value = (*aoffi_immediate >> coord_offsets[coord]) & mask;
        if (value >= wrap_value) {
            value -= diff_value;
        }
        aoffi.push_back(Immediate(value));
    }
    return aoffi;
}

std::vector<Node> ShaderIR::GetPtpCoordinates(std::array<Node, 2> ptp_regs) {
    static constexpr u32 num_entries = 8;

    std::vector<Node> ptp;
    ptp.reserve(num_entries);

    const auto global_size = static_cast<s64>(global_code.size());
    const std::optional low = TrackImmediate(ptp_regs[0], global_code, global_size);
    const std::optional high = TrackImmediate(ptp_regs[1], global_code, global_size);
    if (!low || !high) {
        for (u32 entry = 0; entry < num_entries; ++entry) {
            const u32 reg = entry / 4;
            const u32 offset = entry % 4;
            const Node value = BitfieldExtract(ptp_regs[reg], offset * 8, 6);
            const Node condition =
                Operation(OperationCode::LogicalIGreaterEqual, value, Immediate(32));
            const Node negative = Operation(OperationCode::IAdd, value, Immediate(-64));
            ptp.push_back(Operation(OperationCode::Select, condition, negative, value));
        }
        return ptp;
    }

    const u64 immediate = (static_cast<u64>(*high) << 32) | static_cast<u64>(*low);
    for (u32 entry = 0; entry < num_entries; ++entry) {
        s32 value = (immediate >> (entry * 8)) & 0b111111;
        if (value >= 32) {
            value -= 64;
        }
        ptp.push_back(Immediate(value));
    }

    return ptp;
}

} // namespace VideoCommon::Shader
