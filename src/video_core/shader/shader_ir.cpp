// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>
#include <unordered_map>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::Attribute;
using Tegra::Shader::Instruction;
using Tegra::Shader::IpaMode;
using Tegra::Shader::Pred;
using Tegra::Shader::PredCondition;
using Tegra::Shader::PredOperation;
using Tegra::Shader::Register;

Node ShaderIR::StoreNode(NodeData&& node_data) {
    auto store = std::make_unique<NodeData>(node_data);
    const Node node = store.get();
    stored_nodes.push_back(std::move(store));
    return node;
}

Node ShaderIR::Conditional(Node condition, std::vector<Node>&& code) {
    return StoreNode(ConditionalNode(condition, std::move(code)));
}

Node ShaderIR::Comment(const std::string& text) {
    return StoreNode(CommentNode(text));
}

Node ShaderIR::Immediate(u32 value) {
    return StoreNode(ImmediateNode(value));
}

Node ShaderIR::GetImmediate19(Instruction instr) {
    return Immediate(instr.alu.GetImm20_19());
}

Node ShaderIR::GetImmediate32(Instruction instr) {
    return Immediate(instr.alu.GetImm20_32());
}

Node ShaderIR::GetPredicate(u64 pred_, bool negated) {
    const auto pred = static_cast<Pred>(pred_);
    if (pred != Pred::UnusedIndex && pred != Pred::NeverExecute) {
        used_predicates.insert(pred);
    }

    return StoreNode(PredicateNode(pred, negated));
}

Node ShaderIR::GetPredicate(bool immediate) {
    return GetPredicate(static_cast<u64>(immediate ? Pred::UnusedIndex : Pred::NeverExecute));
}

/*static*/ OperationCode ShaderIR::SignedToUnsignedCode(OperationCode operation_code,
                                                        bool is_signed) {
    if (is_signed) {
        return operation_code;
    }
    switch (operation_code) {
    case OperationCode::FCastInteger:
        return OperationCode::FCastUInteger;
    case OperationCode::IAdd:
        return OperationCode::UAdd;
    case OperationCode::IMul:
        return OperationCode::UMul;
    case OperationCode::IDiv:
        return OperationCode::UDiv;
    case OperationCode::IMin:
        return OperationCode::UMin;
    case OperationCode::IMax:
        return OperationCode::UMax;
    case OperationCode::ICastFloat:
        return OperationCode::UCastFloat;
    case OperationCode::ICastUnsigned:
        return OperationCode::UCastSigned;
    case OperationCode::ILogicalShiftLeft:
        return OperationCode::ULogicalShiftLeft;
    case OperationCode::ILogicalShiftRight:
        return OperationCode::ULogicalShiftRight;
    case OperationCode::IArithmeticShiftRight:
        return OperationCode::UArithmeticShiftRight;
    case OperationCode::IBitwiseAnd:
        return OperationCode::UBitwiseAnd;
    case OperationCode::IBitwiseOr:
        return OperationCode::UBitwiseOr;
    case OperationCode::IBitwiseXor:
        return OperationCode::UBitwiseXor;
    case OperationCode::IBitwiseNot:
        return OperationCode::UBitwiseNot;
    case OperationCode::IBitfieldInsert:
        return OperationCode::UBitfieldInsert;
    case OperationCode::LogicalILessThan:
        return OperationCode::LogicalULessThan;
    case OperationCode::LogicalIEqual:
        return OperationCode::LogicalUEqual;
    case OperationCode::LogicalILessEqual:
        return OperationCode::LogicalULessEqual;
    case OperationCode::LogicalIGreaterThan:
        return OperationCode::LogicalUGreaterThan;
    case OperationCode::LogicalINotEqual:
        return OperationCode::LogicalUNotEqual;
    case OperationCode::LogicalIGreaterEqual:
        return OperationCode::LogicalUGreaterEqual;
    case OperationCode::INegate:
        UNREACHABLE_MSG("Can't negate an unsigned integer");
    case OperationCode::IAbsolute:
        UNREACHABLE_MSG("Can't apply absolute to an unsigned integer");
    }
    UNREACHABLE_MSG("Unknown signed operation with code={}", static_cast<u32>(operation_code));
}

} // namespace VideoCommon::Shader