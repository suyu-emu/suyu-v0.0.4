// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/node_helper.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::Instruction;
using Tegra::Shader::OpCode;

u32 ShaderIR::DecodeBfe(NodeBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    Node op_a = GetRegister(instr.gpr8);
    Node op_b = [&] {
        switch (opcode->get().GetId()) {
        case OpCode::Id::BFE_R:
            return GetRegister(instr.gpr20);
        case OpCode::Id::BFE_C:
            return GetConstBuffer(instr.cbuf34.index, instr.cbuf34.GetOffset());
        case OpCode::Id::BFE_IMM:
            return Immediate(instr.alu.GetSignedImm20_20());
        default:
            UNREACHABLE();
            return Immediate(0);
        }
    }();

    UNIMPLEMENTED_IF_MSG(instr.bfe.rd_cc, "Condition codes in BFE is not implemented");
    UNIMPLEMENTED_IF_MSG(instr.bfe.brev, "BREV in BFE is not implemented");

    const bool is_signed = instr.bfe.is_signed;

    const auto start_position = SignedOperation(OperationCode::IBitfieldExtract, is_signed, op_b,
                                                Immediate(0), Immediate(8));
    const auto bits = SignedOperation(OperationCode::IBitfieldExtract, is_signed, op_b,
                                      Immediate(8), Immediate(8));

    auto result =
        SignedOperation(OperationCode::IBitfieldExtract, is_signed, op_a, start_position, bits);
    SetRegister(bb, instr.gpr0, result);

    return pc;
}

} // namespace VideoCommon::Shader
