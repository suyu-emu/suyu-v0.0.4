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
        break;
    case IR::Opcode::ImageFetch:
        break;
    case IR::Opcode::ImageRead:
        break;
    case IR::Opcode::ImageWrite:
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
