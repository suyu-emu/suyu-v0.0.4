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
using Tegra::Shader::SubOp;

u32 ShaderIR::DecodeArithmetic(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    Node op_a = GetRegister(instr.gpr8);

    Node op_b = [&]() -> Node {
        if (instr.is_b_imm) {
            return GetImmediate19(instr);
        } else if (instr.is_b_gpr) {
            return GetRegister(instr.gpr20);
        } else {
            return GetConstBuffer(instr.cbuf34.index, instr.cbuf34.offset);
        }
    }();

    switch (opcode->get().GetId()) {
    case OpCode::Id::MOV_C:
    case OpCode::Id::MOV_R: {
        // MOV does not have neither 'abs' nor 'neg' bits.
        SetRegister(bb, instr.gpr0, op_b);
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled arithmetic instruction: {}", opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader