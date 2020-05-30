// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/node_helper.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using std::move;
using Tegra::Shader::ConditionCode;
using Tegra::Shader::Instruction;
using Tegra::Shader::IpaInterpMode;
using Tegra::Shader::OpCode;
using Tegra::Shader::PixelImap;
using Tegra::Shader::Register;
using Tegra::Shader::SystemVariable;

using Index = Tegra::Shader::Attribute::Index;

u32 ShaderIR::DecodeOther(NodeBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    switch (opcode->get().GetId()) {
    case OpCode::Id::NOP: {
        UNIMPLEMENTED_IF(instr.nop.cc != Tegra::Shader::ConditionCode::T);
        UNIMPLEMENTED_IF(instr.nop.trigger != 0);
        // With the previous preconditions, this instruction is a no-operation.
        break;
    }
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

        bb.push_back(Operation(OperationCode::Discard));
        break;
    }
    case OpCode::Id::S2R: {
        const Node value = [this, instr] {
            switch (instr.sys20) {
            case SystemVariable::LaneId:
                LOG_WARNING(HW_GPU, "S2R instruction with LaneId is incomplete");
                return Immediate(0U);
            case SystemVariable::InvocationId:
                return Operation(OperationCode::InvocationId);
            case SystemVariable::Ydirection:
                return Operation(OperationCode::YNegate);
            case SystemVariable::InvocationInfo:
                LOG_WARNING(HW_GPU, "S2R instruction with InvocationInfo is incomplete");
                return Immediate(0U);
            case SystemVariable::WscaleFactorXY:
                UNIMPLEMENTED_MSG("S2R WscaleFactorXY is not implemented");
                return Immediate(0U);
            case SystemVariable::WscaleFactorZ:
                UNIMPLEMENTED_MSG("S2R WscaleFactorZ is not implemented");
                return Immediate(0U);
            case SystemVariable::Tid: {
                Node value = Immediate(0);
                value = BitfieldInsert(value, Operation(OperationCode::LocalInvocationIdX), 0, 9);
                value = BitfieldInsert(value, Operation(OperationCode::LocalInvocationIdY), 16, 9);
                value = BitfieldInsert(value, Operation(OperationCode::LocalInvocationIdZ), 26, 5);
                return value;
            }
            case SystemVariable::TidX:
                return Operation(OperationCode::LocalInvocationIdX);
            case SystemVariable::TidY:
                return Operation(OperationCode::LocalInvocationIdY);
            case SystemVariable::TidZ:
                return Operation(OperationCode::LocalInvocationIdZ);
            case SystemVariable::CtaIdX:
                return Operation(OperationCode::WorkGroupIdX);
            case SystemVariable::CtaIdY:
                return Operation(OperationCode::WorkGroupIdY);
            case SystemVariable::CtaIdZ:
                return Operation(OperationCode::WorkGroupIdZ);
            case SystemVariable::EqMask:
            case SystemVariable::LtMask:
            case SystemVariable::LeMask:
            case SystemVariable::GtMask:
            case SystemVariable::GeMask:
                uses_warps = true;
                switch (instr.sys20) {
                case SystemVariable::EqMask:
                    return Operation(OperationCode::ThreadEqMask);
                case SystemVariable::LtMask:
                    return Operation(OperationCode::ThreadLtMask);
                case SystemVariable::LeMask:
                    return Operation(OperationCode::ThreadLeMask);
                case SystemVariable::GtMask:
                    return Operation(OperationCode::ThreadGtMask);
                case SystemVariable::GeMask:
                    return Operation(OperationCode::ThreadGeMask);
                default:
                    UNREACHABLE();
                    return Immediate(0u);
                }
            default:
                UNIMPLEMENTED_MSG("Unhandled system move: {}",
                                  static_cast<u32>(instr.sys20.Value()));
                return Immediate(0u);
            }
        }();
        SetRegister(bb, instr.gpr0, value);

        break;
    }
    case OpCode::Id::BRA: {
        Node branch;
        if (instr.bra.constant_buffer == 0) {
            const u32 target = pc + instr.bra.GetBranchTarget();
            branch = Operation(OperationCode::Branch, Immediate(target));
        } else {
            const u32 target = pc + 1;
            const Node op_a = GetConstBuffer(instr.cbuf36.index, instr.cbuf36.GetOffset());
            const Node convert = SignedOperation(OperationCode::IArithmeticShiftRight, true,
                                                 PRECISE, op_a, Immediate(3));
            const Node operand =
                Operation(OperationCode::IAdd, PRECISE, convert, Immediate(target));
            branch = Operation(OperationCode::BranchIndirect, operand);
        }

        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        if (cc != Tegra::Shader::ConditionCode::T) {
            bb.push_back(Conditional(GetConditionCode(cc), {branch}));
        } else {
            bb.push_back(branch);
        }
        break;
    }
    case OpCode::Id::BRX: {
        Node operand;
        if (instr.brx.constant_buffer != 0) {
            const s32 target = pc + 1;
            const Node index = GetRegister(instr.gpr8);
            const Node op_a =
                GetConstBufferIndirect(instr.cbuf36.index, instr.cbuf36.GetOffset() + 0, index);
            const Node convert = SignedOperation(OperationCode::IArithmeticShiftRight, true,
                                                 PRECISE, op_a, Immediate(3));
            operand = Operation(OperationCode::IAdd, PRECISE, convert, Immediate(target));
        } else {
            const s32 target = pc + instr.brx.GetBranchExtend();
            const Node op_a = GetRegister(instr.gpr8);
            const Node convert = SignedOperation(OperationCode::IArithmeticShiftRight, true,
                                                 PRECISE, op_a, Immediate(3));
            operand = Operation(OperationCode::IAdd, PRECISE, convert, Immediate(target));
        }
        const Node branch = Operation(OperationCode::BranchIndirect, operand);

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

        if (disable_flow_stack) {
            break;
        }

        // The SSY opcode tells the GPU where to re-converge divergent execution paths with SYNC.
        const u32 target = pc + instr.bra.GetBranchTarget();
        bb.push_back(
            Operation(OperationCode::PushFlowStack, MetaStackClass::Ssy, Immediate(target)));
        break;
    }
    case OpCode::Id::PBK: {
        UNIMPLEMENTED_IF_MSG(instr.bra.constant_buffer != 0,
                             "Constant buffer PBK is not supported");

        if (disable_flow_stack) {
            break;
        }

        // PBK pushes to a stack the address where BRK will jump to.
        const u32 target = pc + instr.bra.GetBranchTarget();
        bb.push_back(
            Operation(OperationCode::PushFlowStack, MetaStackClass::Pbk, Immediate(target)));
        break;
    }
    case OpCode::Id::SYNC: {
        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF_MSG(cc != Tegra::Shader::ConditionCode::T, "SYNC condition code used: {}",
                             static_cast<u32>(cc));

        if (decompiled) {
            break;
        }

        // The SYNC opcode jumps to the address previously set by the SSY opcode
        bb.push_back(Operation(OperationCode::PopFlowStack, MetaStackClass::Ssy));
        break;
    }
    case OpCode::Id::BRK: {
        const Tegra::Shader::ConditionCode cc = instr.flow_condition_code;
        UNIMPLEMENTED_IF_MSG(cc != Tegra::Shader::ConditionCode::T, "BRK condition code used: {}",
                             static_cast<u32>(cc));
        if (decompiled) {
            break;
        }

        // The BRK opcode jumps to the address previously set by the PBK opcode
        bb.push_back(Operation(OperationCode::PopFlowStack, MetaStackClass::Pbk));
        break;
    }
    case OpCode::Id::IPA: {
        const bool is_physical = instr.ipa.idx && instr.gpr8.Value() != 0xff;
        const auto attribute = instr.attribute.fmt28;
        const Index index = attribute.index;

        Node value = is_physical ? GetPhysicalInputAttribute(instr.gpr8)
                                 : GetInputAttribute(index, attribute.element);

        // Code taken from Ryujinx.
        if (index >= Index::Attribute_0 && index <= Index::Attribute_31) {
            const u32 location = static_cast<u32>(index) - static_cast<u32>(Index::Attribute_0);
            if (header.ps.GetPixelImap(location) == PixelImap::Perspective) {
                Node position_w = GetInputAttribute(Index::Position, 3);
                value = Operation(OperationCode::FMul, move(value), move(position_w));
            }
        }

        if (instr.ipa.interp_mode == IpaInterpMode::Multiply) {
            value = Operation(OperationCode::FMul, move(value), GetRegister(instr.gpr20));
        }

        value = GetSaturatedFloat(move(value), instr.ipa.saturate);

        SetRegister(bb, instr.gpr0, move(value));
        break;
    }
    case OpCode::Id::OUT_R: {
        UNIMPLEMENTED_IF_MSG(instr.gpr20.Value() != Register::ZeroIndex,
                             "Stream buffer is not supported");

        if (instr.out.emit) {
            // gpr0 is used to store the next address and gpr8 contains the address to emit.
            // Hardware uses pointers here but we just ignore it
            bb.push_back(Operation(OperationCode::EmitVertex));
            SetRegister(bb, instr.gpr0, Immediate(0));
        }
        if (instr.out.cut) {
            bb.push_back(Operation(OperationCode::EndPrimitive));
        }
        break;
    }
    case OpCode::Id::ISBERD: {
        UNIMPLEMENTED_IF(instr.isberd.o != 0);
        UNIMPLEMENTED_IF(instr.isberd.skew != 0);
        UNIMPLEMENTED_IF(instr.isberd.shift != Tegra::Shader::IsberdShift::None);
        UNIMPLEMENTED_IF(instr.isberd.mode != Tegra::Shader::IsberdMode::None);
        LOG_WARNING(HW_GPU, "ISBERD instruction is incomplete");
        SetRegister(bb, instr.gpr0, GetRegister(instr.gpr8));
        break;
    }
    case OpCode::Id::BAR: {
        UNIMPLEMENTED_IF_MSG(instr.value != 0xF0A81B8000070000ULL, "BAR is not BAR.SYNC 0x0");
        bb.push_back(Operation(OperationCode::Barrier));
        break;
    }
    case OpCode::Id::MEMBAR: {
        UNIMPLEMENTED_IF(instr.membar.unknown != Tegra::Shader::MembarUnknown::Default);
        const OperationCode type = [instr] {
            switch (instr.membar.type) {
            case Tegra::Shader::MembarType::CTA:
                return OperationCode::MemoryBarrierGroup;
            case Tegra::Shader::MembarType::GL:
                return OperationCode::MemoryBarrierGlobal;
            default:
                UNIMPLEMENTED_MSG("MEMBAR type={}", static_cast<int>(instr.membar.type.Value()));
                return OperationCode::MemoryBarrierGlobal;
            }
        }();
        bb.push_back(Operation(type));
        break;
    }
    case OpCode::Id::DEPBAR: {
        LOG_DEBUG(HW_GPU, "DEPBAR instruction is stubbed");
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled instruction: {}", opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader
