// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>

#include <fmt/ranges.h>

#include <ranges>
#include "shader_recompiler/frontend/ir/type.h"

namespace Shader::IR {

enum class Opcode {
#define OPCODE(name, ...) name,
#include "opcodes.inc"
#undef OPCODE
};

namespace Detail {
struct OpcodeMeta {
    std::string_view name;
    Type type;
    std::array<Type, 5> arg_types;
};

// using enum Type;
static constexpr Type Void{Type::Void};
static constexpr Type Opaque{Type::Opaque};
static constexpr Type Reg{Type::Reg};
static constexpr Type Pred{Type::Pred};
static constexpr Type Attribute{Type::Attribute};
static constexpr Type Patch{Type::Patch};
static constexpr Type U1{Type::U1};
static constexpr Type U8{Type::U8};
static constexpr Type U16{Type::U16};
static constexpr Type U32{Type::U32};
static constexpr Type U64{Type::U64};
static constexpr Type F16{Type::F16};
static constexpr Type F32{Type::F32};
static constexpr Type F64{Type::F64};
static constexpr Type U32x2{Type::U32x2};
static constexpr Type U32x3{Type::U32x3};
static constexpr Type U32x4{Type::U32x4};
static constexpr Type F16x2{Type::F16x2};
static constexpr Type F16x3{Type::F16x3};
static constexpr Type F16x4{Type::F16x4};
static constexpr Type F32x2{Type::F32x2};
static constexpr Type F32x3{Type::F32x3};
static constexpr Type F32x4{Type::F32x4};
static constexpr Type F64x2{Type::F64x2};
static constexpr Type F64x3{Type::F64x3};
static constexpr Type F64x4{Type::F64x4};

extern OpcodeMeta META_TABLE[532];
constexpr size_t CalculateNumArgsOf(Opcode op) noexcept {
    const auto& arg_types = META_TABLE[size_t(op)].arg_types;
    return size_t(std::distance(arg_types.begin(), std::ranges::find(arg_types, Type::Void)));
}
extern u8 NUM_ARGS[532];
} // namespace Detail

/// Get return type of an opcode
[[nodiscard]] inline Type TypeOf(Opcode op) noexcept {
    return Detail::META_TABLE[size_t(op)].type;
}

/// Get the number of arguments an opcode accepts
[[nodiscard]] inline size_t NumArgsOf(Opcode op) noexcept {
    return size_t(Detail::NUM_ARGS[size_t(op)]);
}

/// Get the required type of an argument of an opcode
[[nodiscard]] inline Type ArgTypeOf(Opcode op, size_t arg_index) noexcept {
    return Detail::META_TABLE[size_t(op)].arg_types[arg_index];
}

/// Get the name of an opcode
[[nodiscard]] std::string_view NameOf(Opcode op);

} // namespace Shader::IR

template <>
struct fmt::formatter<Shader::IR::Opcode> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const Shader::IR::Opcode& op, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", Shader::IR::NameOf(op));
    }
};
