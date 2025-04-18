// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <string_view>

#include "shader_recompiler/backend/msl/emit_msl_instructions.h"
#include "shader_recompiler/backend/msl/msl_emit_context.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::Backend::MSL {
void EmitSelectU1(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                  std::string_view true_value, std::string_view false_value) {
    ctx.AddU1("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectU8([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view cond,
                  [[maybe_unused]] std::string_view true_value,
                  [[maybe_unused]] std::string_view false_value) {
    NotImplemented();
}

void EmitSelectU16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view cond,
                   [[maybe_unused]] std::string_view true_value,
                   [[maybe_unused]] std::string_view false_value) {
    NotImplemented();
}

void EmitSelectU32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddU32("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectU64(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddU64("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectF16([[maybe_unused]] EmitContext& ctx, [[maybe_unused]] std::string_view cond,
                   [[maybe_unused]] std::string_view true_value,
                   [[maybe_unused]] std::string_view false_value) {
    NotImplemented();
}

void EmitSelectF32(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddF32("{}={}?{}:{};", inst, cond, true_value, false_value);
}

void EmitSelectF64(EmitContext& ctx, IR::Inst& inst, std::string_view cond,
                   std::string_view true_value, std::string_view false_value) {
    ctx.AddF64("{}={}?{}:{};", inst, cond, true_value, false_value);
}

} // namespace Shader::Backend::MSL
