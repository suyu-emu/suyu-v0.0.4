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
    case OpCode::Id::KIL: {
        UNIMPLEMENTED_IF(instr.flow.cond != Tegra::Shader::FlowCondition::Always);

        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF_MSG(cc != Tegra::Shader::ConditionCode::T, "KIL condition code used: {}",
                             static_cast<u32>(cc));

        bb.push_back(Operation(OperationCode::Kil));
        break;
    }
    case OpCode::Id::MOV_SYS: {
        switch (instr.sys20) {
        case Tegra::Shader::SystemVariable::InvocationInfo: {
            LOG_WARNING(HW_GPU, "MOV_SYS instruction with InvocationInfo is incomplete");
            SetRegister(bb, instr.gpr0, Immediate(0u));
            break;
        }
        case Tegra::Shader::SystemVariable::Ydirection: {
            // Config pack's third value is Y_NEGATE's state.
            SetRegister(bb, instr.gpr0, Operation(OperationCode::YNegate));
            break;
        }
        default:
            UNIMPLEMENTED_MSG("Unhandled system move: {}", static_cast<u32>(instr.sys20.Value()));
        }
        break;
    }
    case OpCode::Id::BRA: {
        UNIMPLEMENTED_IF_MSG(instr.bra.constant_buffer != 0,
                             "BRA with constant buffers are not implemented");

        const u32 target = pc + instr.bra.GetBranchTarget();
        const Node branch = Operation(OperationCode::Bra, Immediate(target));

        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        if (cc != Tegra::Shader::ConditionCode::T) {
            bb.push_back(Conditional(GetConditionCode(cc), {branch}));
        } else {
            bb.push_back(branch);
        }
        break;
    }
    case OpCode::Id::SSY: {
        UNIMPLEMENTED_IF_MSG(instr.bra.constant_buffer != 0,
                             "Constant buffer flow is not supported");

        // The SSY opcode tells the GPU where to re-converge divergent execution paths, it sets the
        // target of the jump that the SYNC instruction will make. The SSY opcode has a similar
        // structure to the BRA opcode.
        const u32 target = pc + instr.bra.GetBranchTarget();
        bb.push_back(Operation(OperationCode::Ssy, Immediate(target)));
        break;
    }
    case OpCode::Id::PBK: {
        UNIMPLEMENTED_IF_MSG(instr.bra.constant_buffer != 0,
                             "Constant buffer PBK is not supported");

        // PBK pushes to a stack the address where BRK will jump to. This shares stack with SSY but
        // using SYNC on a PBK address will kill the shader execution. We don't emulate this because
        // it's very unlikely a driver will emit such invalid shader.
        const u32 target = pc + instr.bra.GetBranchTarget();
        bb.push_back(Operation(OperationCode::Pbk, Immediate(target)));
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
    case OpCode::Id::BRK: {
        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF_MSG(cc != Tegra::Shader::ConditionCode::T, "BRK condition code used: {}",
                             static_cast<u32>(cc));

        // The BRK opcode jumps to the address previously set by the PBK opcode
        bb.push_back(Operation(OperationCode::Brk));
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
    case OpCode::Id::DEPBAR: {
        LOG_WARNING(HW_GPU, "DEPBAR instruction is stubbed");
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled instruction: {}", opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader