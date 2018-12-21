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

Node ShaderIR::GetRegister(Register reg) {
    if (reg != Register::ZeroIndex) {
        used_registers.insert(static_cast<u32>(reg));
    }
    return StoreNode(GprNode(reg));
}

Node ShaderIR::GetImmediate19(Instruction instr) {
    return Immediate(instr.alu.GetImm20_19());
}

Node ShaderIR::GetImmediate32(Instruction instr) {
    return Immediate(instr.alu.GetImm20_32());
}

Node ShaderIR::GetConstBuffer(u64 index_, u64 offset_) {
    const auto index = static_cast<u32>(index_);
    const auto offset = static_cast<u32>(offset_);

    const auto [entry, is_new] = used_cbufs.try_emplace(index);
    entry->second.MarkAsUsed(offset);

    return StoreNode(CbufNode(index, Immediate(offset)));
}

Node ShaderIR::GetConstBufferIndirect(u64 index_, u64 offset_, Node node) {
    const auto index = static_cast<u32>(index_);
    const auto offset = static_cast<u32>(offset_);

    const auto [entry, is_new] = used_cbufs.try_emplace(index);
    entry->second.MarkAsUsedIndirect();

    const Node final_offset = Operation(OperationCode::UAdd, NO_PRECISE, node, Immediate(offset));
    return StoreNode(CbufNode(index, final_offset));
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

Node ShaderIR::GetInputAttribute(Attribute::Index index, u64 element,
                                 const Tegra::Shader::IpaMode& input_mode, Node buffer) {
    const auto [entry, is_new] =
        used_input_attributes.emplace(std::make_pair(index, std::set<Tegra::Shader::IpaMode>{}));
    entry->second.insert(input_mode);

    return StoreNode(AbufNode(index, static_cast<u32>(element), input_mode, buffer));
}

Node ShaderIR::GetOutputAttribute(Attribute::Index index, u64 element, Node buffer) {
    if (index == Attribute::Index::ClipDistances0123 ||
        index == Attribute::Index::ClipDistances4567) {
        const auto clip_index =
            static_cast<u32>((index == Attribute::Index::ClipDistances4567 ? 1 : 0) + element);
        used_clip_distances.at(clip_index) = true;
    }
    used_output_attributes.insert(index);

    return StoreNode(AbufNode(index, static_cast<u32>(element), buffer));
}

Node ShaderIR::GetInternalFlag(InternalFlag flag, bool negated) {
    const Node node = StoreNode(InternalFlagNode(flag));
    if (negated) {
        return Operation(OperationCode::LogicalNegate, node);
    }
    return node;
}

Node ShaderIR::GetLocalMemory(Node address) {
    return StoreNode(LmemNode(address));
}

Node ShaderIR::GetOperandAbsNegFloat(Node value, bool absolute, bool negate) {
    if (absolute) {
        value = Operation(OperationCode::FAbsolute, NO_PRECISE, value);
    }
    if (negate) {
        value = Operation(OperationCode::FNegate, NO_PRECISE, value);
    }
    return value;
}

Node ShaderIR::GetSaturatedFloat(Node value, bool saturate) {
    if (!saturate) {
        return value;
    }
    const Node positive_zero = Immediate(std::copysignf(0, 1));
    const Node positive_one = Immediate(1.0f);
    return Operation(OperationCode::FClamp, NO_PRECISE, value, positive_zero, positive_one);
}

Node ShaderIR::ConvertIntegerSize(Node value, Tegra::Shader::Register::Size size, bool is_signed) {
    switch (size) {
    case Register::Size::Byte:
        value = SignedOperation(OperationCode::ILogicalShiftLeft, is_signed, NO_PRECISE, value,
                                Immediate(24));
        value = SignedOperation(OperationCode::IArithmeticShiftRight, is_signed, NO_PRECISE, value,
                                Immediate(24));
        return value;
    case Register::Size::Short:
        value = SignedOperation(OperationCode::ILogicalShiftLeft, is_signed, NO_PRECISE, value,
                                Immediate(16));
        value = SignedOperation(OperationCode::IArithmeticShiftRight, is_signed, NO_PRECISE, value,
                                Immediate(16));
    case Register::Size::Word:
        // Default - do nothing
        return value;
    default:
        UNREACHABLE_MSG("Unimplemented conversion size: {}", static_cast<u32>(size));
    }
}

Node ShaderIR::GetOperandAbsNegInteger(Node value, bool absolute, bool negate, bool is_signed) {
    if (!is_signed) {
        // Absolute or negate on an unsigned is pointless
        return value;
    }
    if (absolute) {
        value = Operation(OperationCode::IAbsolute, NO_PRECISE, value);
    }
    if (negate) {
        value = Operation(OperationCode::INegate, NO_PRECISE, value);
    }
    return value;
}

void ShaderIR::SetRegister(BasicBlock& bb, Register dest, Node src) {
    bb.push_back(Operation(OperationCode::Assign, GetRegister(dest), src));
}

void ShaderIR::SetPredicate(BasicBlock& bb, u64 dest, Node src) {
    bb.push_back(Operation(OperationCode::LogicalAssign, GetPredicate(dest), src));
}

void ShaderIR::SetInternalFlag(BasicBlock& bb, InternalFlag flag, Node value) {
    bb.push_back(Operation(OperationCode::LogicalAssign, GetInternalFlag(flag), value));
}

void ShaderIR::SetLocalMemory(BasicBlock& bb, Node address, Node value) {
    bb.push_back(Operation(OperationCode::Assign, GetLocalMemory(address), value));
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