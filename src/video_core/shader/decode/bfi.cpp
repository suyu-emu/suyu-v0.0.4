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

u32 ShaderIR::DecodeBfi(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    UNIMPLEMENTED_IF(instr.generates_cc);

    const auto [base, packed_shift] = [&]() -> std::tuple<Node, Node> {
        switch (opcode->get().GetId()) {
        case OpCode::Id::BFI_IMM_R:
            return {GetRegister(instr.gpr39), Immediate(instr.alu.GetSignedImm20_20())};
        default:
            UNREACHABLE();
        }
    }();
    const Node insert = GetRegister(instr.gpr8);

    const Node offset =
        Operation(OperationCode::UBitwiseAnd, NO_PRECISE, packed_shift, Immediate(0xff));

    Node bits =
        Operation(OperationCode::ULogicalShiftRight, NO_PRECISE, packed_shift, Immediate(8));
    bits = Operation(OperationCode::UBitwiseAnd, NO_PRECISE, bits, Immediate(0xff));

    const Node value =
        Operation(OperationCode::UBitfieldInsert, PRECISE, base, insert, offset, bits);
    SetRegister(bb, instr.gpr0, value);

    return pc;
}

} // namespace VideoCommon::Shader