// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::Instruction;
using Tegra::Shader::OpCode;
using Tegra::Shader::ConditionCode;

u32 ShaderIR::DecodeOther(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    switch (opcode->get().GetId()) {
    case OpCode::Id::EXIT: {
        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF_MSG(cc != Tegra::Shader::ConditionCode::T, "EXIT condition code used: {}",
                             static_cast<u32>(cc));

        switch (instr.flow.cond) {
        case Tegra::Shader::FlowCondition::Always:
            bb.push_back(Operation(OperationCode::Exit));
            if (instr.pred.pred_index == static_cast<u64>(Tegra::Shader::Pred::UnusedIndex)) {
                // If this is an unconditional exit then just end processing here,
                // otherwise we have to account for the possibility of the condition
                // not being met, so continue processing the next instruction.
                pc = MAX_PROGRAM_LENGTH - 1;
            }
            break;

        case Tegra::Shader::FlowCondition::Fcsm_Tr:
            // TODO(bunnei): What is this used for? If we assume this conditon is not
            // satisifed, dual vertex shaders in Farming Simulator make more sense
            UNIMPLEMENTED_MSG("Skipping unknown FlowCondition::Fcsm_Tr");
            break;

        default:
            UNIMPLEMENTED_MSG("Unhandled flow condition: {}",
                              static_cast<u32>(instr.flow.cond.Value()));
        }
        break;
    }
    case OpCode::Id::BRA: {
        UNIMPLEMENTED_IF_MSG(instr.bra.constant_buffer != 0,
                             "BRA with constant buffers are not implemented");

        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF(cc != Tegra::Shader::ConditionCode::T);

        const u32 target = pc + instr.bra.GetBranchTarget();
        bb.push_back(Operation(OperationCode::Bra, Immediate(target)));
        break;
    }
    case OpCode::Id::SSY: {
        UNIMPLEMENTED_IF_MSG(instr.bra.constant_buffer != 0,
                             "Constant buffer flow is not supported");

        // The SSY opcode tells the GPU where to re-converge divergent execution paths, it sets the
        // target of the jump that the SYNC instruction will make. The SSY opcode has a similar
        // structure to the BRA opcode.
        bb.push_back(Operation(OperationCode::Ssy, Immediate(pc + instr.bra.GetBranchTarget())));
        break;
    }
    case OpCode::Id::SYNC: {
        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF_MSG(cc != Tegra::Shader::ConditionCode::T, "SYNC condition code used: {}",
                             static_cast<u32>(cc));

        // The SYNC opcode jumps to the address previously set by the SSY opcode
        bb.push_back(Operation(OperationCode::Sync));
        break;
    }
    case OpCode::Id::IPA: {
        const auto& attribute = instr.attribute.fmt28;
        const Tegra::Shader::IpaMode input_mode{instr.ipa.interp_mode.Value(),
                                                instr.ipa.sample_mode.Value()};

        const Node input_attr = GetInputAttribute(attribute.index, attribute.element, input_mode);
        const Node ipa = Operation(OperationCode::Ipa, input_attr);
        const Node value = GetSaturatedFloat(ipa, instr.ipa.saturate);

        SetRegister(bb, instr.gpr0, value);
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled instruction: {}", opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader