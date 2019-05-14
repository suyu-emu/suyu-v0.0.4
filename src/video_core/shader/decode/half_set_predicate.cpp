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
using Tegra::Shader::Pred;

u32 ShaderIR::DecodeHalfSetPredicate(NodeBlock& bb, u32 pc) {
    const Instruction instr = {program_code[pc]};
    const auto opcode = OpCode::Decode(instr);

    UNIMPLEMENTED_IF(instr.hsetp2.ftz != 0);

    Node op_a = UnpackHalfFloat(GetRegister(instr.gpr8), instr.hsetp2.type_a);
    op_a = GetOperandAbsNegHalf(op_a, instr.hsetp2.abs_a, instr.hsetp2.negate_a);

    Tegra::Shader::PredCondition cond{};
    bool h_and{};
    Node op_b{};
    switch (opcode->get().GetId()) {
    case OpCode::Id::HSETP2_C:
        cond = instr.hsetp2.cbuf_and_imm.cond;
        h_and = instr.hsetp2.cbuf_and_imm.h_and;
        op_b = GetOperandAbsNegHalf(GetConstBuffer(instr.cbuf34.index, instr.cbuf34.offset),
                                    instr.hsetp2.cbuf.abs_b, instr.hsetp2.cbuf.negate_b);
        break;
    case OpCode::Id::HSETP2_IMM:
        cond = instr.hsetp2.cbuf_and_imm.cond;
        h_and = instr.hsetp2.cbuf_and_imm.h_and;
        op_b = UnpackHalfImmediate(instr, true);
        break;
    case OpCode::Id::HSETP2_R:
        cond = instr.hsetp2.reg.cond;
        h_and = instr.hsetp2.reg.h_and;
        op_b =
            UnpackHalfFloat(GetOperandAbsNegHalf(GetRegister(instr.gpr20), instr.hsetp2.reg.abs_b,
                                                 instr.hsetp2.reg.negate_b),
                            instr.hsetp2.reg.type_b);
        break;
    default:
        UNREACHABLE();
        op_b = Immediate(0);
    }

    // We can't use the constant predicate as destination.
    ASSERT(instr.hsetp2.pred3 != static_cast<u64>(Pred::UnusedIndex));

    const Node second_pred = GetPredicate(instr.hsetp2.pred39, instr.hsetp2.neg_pred != 0);

    const OperationCode combiner = GetPredicateCombiner(instr.hsetp2.op);
    const OperationCode pair_combiner =
        h_and ? OperationCode::LogicalAll2 : OperationCode::LogicalAny2;

    const Node comparison = GetPredicateComparisonHalf(cond, op_a, op_b);
    const Node first_pred = Operation(pair_combiner, comparison);

    // Set the primary predicate to the result of Predicate OP SecondPredicate
    const Node value = Operation(combiner, first_pred, second_pred);
    SetPredicate(bb, instr.hsetp2.pred3, value);

    if (instr.hsetp2.pred0 != static_cast<u64>(Pred::UnusedIndex)) {
        // Set the secondary predicate to the result of !Predicate OP SecondPredicate, if enabled
        const Node negated_pred = Operation(OperationCode::LogicalNegate, first_pred);
        SetPredicate(bb, instr.hsetp2.pred0, Operation(combiner, negated_pred, second_pred));
    }

    return pc;
}

} // namespace VideoCommon::Shader
