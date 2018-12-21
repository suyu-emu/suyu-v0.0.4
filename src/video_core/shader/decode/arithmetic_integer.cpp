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
    default:
        UNIMPLEMENTED_MSG("Unhandled ArithmeticInteger instruction: {}", opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader