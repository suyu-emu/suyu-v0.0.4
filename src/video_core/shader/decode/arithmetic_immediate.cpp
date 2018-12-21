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

u32 ShaderIR::DecodeArithmeticImmediate(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    switch (opcode->get().GetId()) {
    case OpCode::Id::MOV32_IMM: {
        SetRegister(bb, instr.gpr0, GetImmediate32(instr));
        break;
    }
    default:
        UNIMPLEMENTED_MSG("Unhandled arithmetic immediate instruction: {}",
                          opcode->get().GetName());
    }

    return pc;
}

} // namespace VideoCommon::Shader