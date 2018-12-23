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

u32 ShaderIR::DecodeRegisterSetPredicate(BasicBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    UNIMPLEMENTED_IF(instr.r2p.mode != Tegra::Shader::R2pMode::Pr);

    const Node apply_mask = [&]() {
        switch (opcode->get().GetId()) {
        case OpCode::Id::R2P_IMM:
            return Immediate(static_cast<u32>(instr.r2p.immediate_mask));
        default:
            UNREACHABLE();
            return Immediate(static_cast<u32>(instr.r2p.immediate_mask));
        }
    }();
    const Node mask =
        Operation(OperationCode::ULogicalShiftRight, NO_PRECISE, GetRegister(instr.gpr8),
                  Immediate(static_cast<u32>(instr.r2p.byte)));

    constexpr u32 programmable_preds = 7;
    for (u64 pred = 0; pred < programmable_preds; ++pred) {
        const Node shift = Immediate(1u << static_cast<u32>(pred));

        const Node apply_compare = Operation(OperationCode::UBitwiseAnd, NO_PRECISE, apply_mask, shift);
        const Node condition = Operation(OperationCode::LogicalUEqual, apply_compare, Immediate(0));

        const Node value_compare = Operation(OperationCode::UBitwiseAnd, NO_PRECISE, mask, shift);
        const Node value = Operation(OperationCode::LogicalUEqual, value_compare, Immediate(0));

        const Node code = Operation(OperationCode::LogicalAssign, GetPredicate(pred), value);
        bb.push_back(Conditional(condition, {code}));
    }

    return pc;
}

} // namespace VideoCommon::Shader