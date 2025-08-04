/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/ir/opcodes.h"

#include <array>
#include <vector>

#include "dynarmic/ir/type.h"

namespace Dynarmic::IR {

// Opcode information

namespace OpcodeInfo {

struct Meta {
    std::vector<Type> arg_types;
    const char* name;
    Type type;
};

constexpr Type Void = Type::Void;
constexpr Type A32Reg = Type::A32Reg;
constexpr Type A32ExtReg = Type::A32ExtReg;
constexpr Type A64Reg = Type::A64Reg;
constexpr Type A64Vec = Type::A64Vec;
constexpr Type Opaque = Type::Opaque;
constexpr Type U1 = Type::U1;
constexpr Type U8 = Type::U8;
constexpr Type U16 = Type::U16;
constexpr Type U32 = Type::U32;
constexpr Type U64 = Type::U64;
constexpr Type U128 = Type::U128;
constexpr Type CoprocInfo = Type::CoprocInfo;
constexpr Type NZCV = Type::NZCVFlags;
constexpr Type Cond = Type::Cond;
constexpr Type Table = Type::Table;
constexpr Type AccType = Type::AccType;

alignas(64) static const std::array opcode_info{
#define OPCODE(name, type, ...) Meta{{__VA_ARGS__}, #name, type},
#define A32OPC(name, type, ...) Meta{{__VA_ARGS__}, #name, type},
#define A64OPC(name, type, ...) Meta{{__VA_ARGS__}, #name, type},
#include "./opcodes.inc"
#undef OPCODE
#undef A32OPC
#undef A64OPC
};

}  // namespace OpcodeInfo

/// @brief Get return type of an opcode
Type GetTypeOf(Opcode op) noexcept {
    return OpcodeInfo::opcode_info.at(size_t(op)).type;
}

/// @brief Get the number of arguments an opcode accepts
size_t GetNumArgsOf(Opcode op) noexcept {
    return OpcodeInfo::opcode_info.at(size_t(op)).arg_types.size();
}

/// @brief Get the required type of an argument of an opcode
Type GetArgTypeOf(Opcode op, size_t arg_index) noexcept {
    return OpcodeInfo::opcode_info.at(size_t(op)).arg_types.at(arg_index);
}

/// @brief Get the name of an opcode.
std::string GetNameOf(Opcode op) noexcept {
    return OpcodeInfo::opcode_info.at(size_t(op)).name;
}

}  // namespace Dynarmic::IR
