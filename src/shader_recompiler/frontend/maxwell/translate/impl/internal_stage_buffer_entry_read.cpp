// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {
namespace {
enum class Mode : u64 {
    Default,
    Patch,
    Prim,
    Attr,
};

enum class Shift : u64 {
    Default,
    U16,
    B32,
};

} // Anonymous namespace

// Valid only for GS, TI, VS and trap
void TranslatorVisitor::ISBERD(u64 insn) {
    union {
        u64 raw;
        BitField<0, 8, IR::Reg> dest_reg;
        BitField<8, 8, IR::Reg> src_reg;
        BitField<8, 8, u32> src_reg_num;
        BitField<24, 8, u32> imm;
        BitField<31, 1, u64> skew;
        BitField<32, 1, u64> o;
        BitField<33, 2, Mode> mode;
        BitField<47, 2, Shift> shift;
    } const isberd{insn};

    if (isberd.skew != 0) {
        throw NotImplementedException("SKEW");
    }
    if (isberd.o != 0) {
        throw NotImplementedException("O");
    }
    if (isberd.mode != Mode::Default) {
        throw NotImplementedException("Mode {}", isberd.mode.Value());
    }
    if (isberd.shift != Shift::Default) {
        throw NotImplementedException("Shift {}", isberd.shift.Value());
    }
    //LOG_DEBUG(Shader, "(STUBBED) called {}", insn);
    if (isberd.src_reg_num == 0xFF) {
        IR::U32 src_imm{ir.Imm32(static_cast<u32>(isberd.imm))};
        IR::U32 result{ir.IAdd(X(isberd.src_reg), src_imm)};
        X(isberd.dest_reg, result);
    } else {
        X(isberd.dest_reg, X(isberd.src_reg));
    }
}

} // namespace Shader::Maxwell
