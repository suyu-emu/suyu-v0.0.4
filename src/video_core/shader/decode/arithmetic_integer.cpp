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

u32 ShaderIR::DecodeArithmeticInteger(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    Node op_a = GetRegister(instr.gpr8);
    Node op_b = [&]() {
        if (instr.is_b_imm) {
            return Immediate(instr.alu.GetSignedImm20_20());
        } else if (instr.is_b_gpr) {
            return GetRegister(instr.gpr20);
        } else {
            return GetConstBuffer(instr.cbuf34.index, instr.cbuf34.offset);
        }
    }();

    switch (opcode->get().GetId()) {
    case OpCode::Id::IADD_C:
    case OpCode::Id::IADD_R:
    case OpCode::Id::IADD_IMM: {
        UNIMPLEMENTED_IF_MSG(instr.generates_cc,
                             "Condition codes generation in IADD is not implemented");
        UNIMPLEMENTED_IF_MSG(instr.alu.saturate_d, "IADD saturation not implemented");

        op_a = GetOperandAbsNegInteger(op_a, false, instr.alu_integer.negate_a, true);
        op_b = GetOperandAbsNegInteger(op_b, false, instr.alu_integer.negate_b, true);

        SetRegister(bb, instr.gpr0, Operation(OperationCode::IAdd, PRECISE, op_a, op_b));
        break;
    }
    case OpCode::Id::ISCADD_C:
    case OpCode::Id::ISCADD_R:
    case OpCode::Id::ISCADD_IMM: {
        UNIMPLEMENTED_IF_MSG(instr.generates_cc,
                             "Condition codes generation in ISCADD is not implemented");

        op_a = GetOperandAbsNegInteger(op_a, false, instr.alu_integer.negate_a, true);
        op_b = GetOperandAbsNegInteger(op_b, false, instr.alu_integer.negate_b, true);

        const Node shift = Immediate(static_cast<u32>(instr.alu_integer.shift_amount));
        const Node shifted_a = Operation(OperationCode::ILogicalShiftLeft, NO_PRECISE, op_a, shift);
        const Node value = Operation(OperationCode::IAdd, NO_PRECISE, shifted_a, op_b);
        SetRegister(bb, instr.gpr0, value);
        break;
    }
    case OpCode::Id::SEL_C:
    case OpCode::Id::SEL_R:
    case OpCode::Id::SEL_IMM: {
        const Node condition = GetPredicate(instr.sel.pred, instr.sel.neg_pred != 0);
        const Node value = Operation(OperationCode::Select, PRECISE, condition, op_a, op_b);
        SetRegister(bb, instr.gpr0, value);
        break;
    }
    case OpCode::Id::LOP_C:
    case OpCode::Id::LOP_R:
    case OpCode::Id::LOP_IMM: {
        UNIMPLEMENTED_IF_MSG(instr.generates_cc,
                             "Condition codes generation in LOP is not implemented");

        if (instr.alu.lop.invert_a)
            op_a = Operation(OperationCode::IBitwiseNot, NO_PRECISE, op_a);
        if (instr.alu.lop.invert_b)
            op_b = Operation(OperationCode::IBitwiseNot, NO_PRECISE, op_b);

        WriteLogicOperation(bb, instr.gpr0, instr.alu.lop.operation, op_a, op_b,
                            instr.alu.lop.pred_result_mode, instr.alu.lop.pred48);
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled ArithmeticInteger instruction: {}", opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader