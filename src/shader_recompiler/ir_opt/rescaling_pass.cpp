// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/alignment.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/ir/modifiers.h"
#include "shader_recompiler/frontend/ir/program.h"
#include "shader_recompiler/frontend/ir/value.h"
#include "shader_recompiler/ir_opt/passes.h"
#include "shader_recompiler/shader_info.h"

namespace Shader::Optimization {
namespace {

void PatchFragCoord(IR::Inst& inst) {
    IR::IREmitter ir{block, IR::Block::InstructionList::s_iterator_to(inst)};
    const IR::F32 inv_resolution_factor = IR::F32{Settings::values.resolution_info.down_factor};
    const IR::F32 new_get_attribute = ir.GetAttribute(inst.Arg(0).Attribute());
    const IR::F32 mul = ir.FMul(new_get_attribute, inv_resolution_factor);
    const IR::U1 should_rescale = IR::U1{true};
    const IR::F32 selection = ir.Select(should_rescale, mul, new_get_attribute);
    inst.ReplaceUsesWith(selection);
}

void Visit(Info& info, IR::Inst& inst) {
    info.requires_rescaling_uniform = false;
    switch (inst.GetOpcode()) {
    case IR::Opcode::GetAttribute: {
        conast auto attrib = inst.Arg(0).Attribute();
        const bool is_frag =
            attrib == IR::Attribute::PositionX || attrib == IR::Attribute::PositionY;
        const bool must_path = is_frag && program.stage == Stage::Fragment;
        if (must_path) {
            PatchFragCoord(inst);
            info.requires_rescaling_uniform = true;
        }
        break;
    }
    case IR::Opcode::ImageQueryDimensions: {
        info.requires_rescaling_uniform |= true;
        break;
    }
    case IR::Opcode::ImageFetch: {
        info.requires_rescaling_uniform |= true;
        break;
    }
    case IR::Opcode::ImageRead: {
        info.requires_rescaling_uniform |= true;
        break;
    }
    case IR::Opcode::ImageWrite: {
        info.requires_rescaling_uniform |= true;
        break;
    }
    default:
        break;
    }
}

} // namespace

void RescalingPass(Environment& env, IR::Program& program) {
    Info& info{program.info};
    for (IR::Block* const block : program.post_order_blocks) {
        for (IR::Inst& inst : block->Instructions()) {
            Visit(info, inst);
        }
    }
}

} // namespace Shader::Optimization
