// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "common/settings.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/ir_emitter.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Optimization {
namespace {
void PatchFragCoord(IR::Block& block, IR::Inst& inst) {
    IR::IREmitter ir{block, IR::Block::InstructionList::s_iterator_to(inst)};
    const IR::F32 down_factor{ir.ResolutionDownFactor()};
    const IR::F32 frag_coord{ir.GetAttribute(inst.Arg(0).Attribute())};
    const IR::F32 downscaled_frag_coord{ir.FPMul(frag_coord, down_factor)};
    inst.ReplaceUsesWith(downscaled_frag_coord);
}

[[nodiscard]] IR::U32 Scale(IR::IREmitter& ir, const IR::U1& is_scaled, const IR::U32& value) {
    IR::U32 scaled_value{value};
    bool changed{};
    if (const u32 up_scale = Settings::values.resolution_info.up_scale; up_scale != 1) {
        scaled_value = ir.IMul(value, ir.Imm32(up_scale));
        changed = true;
    }
    if (const u32 down_shift = Settings::values.resolution_info.down_shift; down_shift != 0) {
        scaled_value = ir.ShiftRightArithmetic(value, ir.Imm32(down_shift));
        changed = true;
    }
    if (changed) {
        return IR::U32{ir.Select(is_scaled, scaled_value, value)};
    } else {
        return value;
    }
}

[[nodiscard]] IR::U32 DownScale(IR::IREmitter& ir, IR::U32 value) {
    if (const u32 down_shift = Settings::values.resolution_info.down_shift; down_shift != 0) {
        value = ir.ShiftLeftLogical(value, ir.Imm32(down_shift));
    }
    if (const u32 up_scale = Settings::values.resolution_info.up_scale; up_scale != 1) {
        value = ir.IDiv(value, ir.Imm32(up_scale));
    }
    return value;
}

void PatchImageQueryDimensions(IR::Block& block, IR::Inst& inst) {
    const auto it{IR::Block::InstructionList::s_iterator_to(inst)};
    IR::IREmitter ir{block, IR::Block::InstructionList::s_iterator_to(inst)};
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    switch (info.type) {
    case TextureType::Color1D:
    case TextureType::ColorArray1D: {
        const IR::Value new_inst{&*block.PrependNewInst(it, inst)};
        const IR::U32 width{DownScale(ir, IR::U32{ir.CompositeExtract(new_inst, 0)})};
        const IR::Value replacement{ir.CompositeConstruct(width, ir.CompositeExtract(new_inst, 1),
                                                          ir.CompositeExtract(new_inst, 2),
                                                          ir.CompositeExtract(new_inst, 3))};
        inst.ReplaceUsesWith(replacement);
        break;
    }
    case TextureType::Color2D:
    case TextureType::ColorArray2D: {
        const IR::Value new_inst{&*block.PrependNewInst(it, inst)};
        const IR::U32 width{DownScale(ir, IR::U32{ir.CompositeExtract(new_inst, 0)})};
        const IR::U32 height{DownScale(ir, IR::U32{ir.CompositeExtract(new_inst, 1)})};
        const IR::Value replacement{ir.CompositeConstruct(
            width, height, ir.CompositeExtract(new_inst, 2), ir.CompositeExtract(new_inst, 3))};
        inst.ReplaceUsesWith(replacement);
        break;
    }
    case TextureType::Color3D:
    case TextureType::ColorCube:
    case TextureType::ColorArrayCube:
    case TextureType::Buffer:
        // Nothing to patch here
        break;
    }
}

void PatchImageFetch(IR::Block& block, IR::Inst& inst) {
    IR::IREmitter ir{block, IR::Block::InstructionList::s_iterator_to(inst)};
    const auto info{inst.Flags<IR::TextureInstInfo>()};
    const IR::U1 is_scaled{ir.IsTextureScaled(ir.Imm32(info.descriptor_index))};
    const IR::Value coord{inst.Arg(1)};
    switch (info.type) {
    case TextureType::Color1D:
        inst.SetArg(1, Scale(ir, is_scaled, IR::U32{coord}));
        break;
    case TextureType::ColorArray1D: {
        const IR::U32 x{Scale(ir, is_scaled, IR::U32{ir.CompositeExtract(coord, 0)})};
        const IR::U32 y{ir.CompositeExtract(coord, 1)};
        inst.SetArg(1, ir.CompositeConstruct(x, y));
        break;
    }
    case TextureType::Color2D: {
        const IR::U32 x{Scale(ir, is_scaled, IR::U32{ir.CompositeExtract(coord, 0)})};
        const IR::U32 y{Scale(ir, is_scaled, IR::U32{ir.CompositeExtract(coord, 1)})};
        inst.SetArg(1, ir.CompositeConstruct(x, y));
        break;
    }
    case TextureType::ColorArray2D: {
        const IR::U32 x{Scale(ir, is_scaled, IR::U32{ir.CompositeExtract(coord, 0)})};
        const IR::U32 y{Scale(ir, is_scaled, IR::U32{ir.CompositeExtract(coord, 1)})};
        const IR::U32 z{ir.CompositeExtract(coord, 2)};
        inst.SetArg(1, ir.CompositeConstruct(x, y, z));
        break;
    }
    case TextureType::Color3D:
    case TextureType::ColorCube:
    case TextureType::ColorArrayCube:
    case TextureType::Buffer:
        // Nothing to patch here
        break;
    }
}

void Visit(const IR::Program& program, IR::Block& block, IR::Inst& inst) {
    const bool is_fragment_shader{program.stage == Stage::Fragment};
    switch (inst.GetOpcode()) {
    case IR::Opcode::GetAttribute: {
        const IR::Attribute attr{inst.Arg(0).Attribute()};
        switch (attr) {
        case IR::Attribute::PositionX:
        case IR::Attribute::PositionY:
            if (is_fragment_shader) {
                PatchFragCoord(block, inst);
            }
            break;
        default:
            break;
        }
        break;
    }
    case IR::Opcode::ImageQueryDimensions:
        PatchImageQueryDimensions(block, inst);
        break;
    case IR::Opcode::ImageFetch:
        PatchImageFetch(block, inst);
        break;
    default:
        break;
    }
}
} // Anonymous namespace

void RescalingPass(IR::Program& program) {
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            Visit(program, *block, inst);
        }
    }
}

} // namespace Shader::Optimization
