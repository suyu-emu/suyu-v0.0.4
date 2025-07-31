// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

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
        IR::U32 current_lane_id{ir.LaneId()};
        IR::U32 result{ir.IAdd(X(isberd.src_reg), current_lane_id)};
        X(isberd.dest_reg, result);
    }
    if (isberd.o != 0) {
        IR::U32 address{};
        IR::F32 result{};
        if (isberd.src_reg_num == 0xFF) {
            address = ir.Imm32(isberd.imm);
            result = ir.GetAttributeIndexed(address);
        } else {
            IR::U32 offset = ir.Imm32(isberd.imm);
            address = ir.IAdd(X(isberd.src_reg), offset);
            result = ir.GetAttributeIndexed(address);
        }
        X(isberd.dest_reg, ir.BitCast<IR::U32>(result));
    }
    if (isberd.mode != Mode::Default) {
        IR::F32 result{};
        IR::U32 index{};
        if (isberd.src_reg_num == 0xFF) {
            index = ir.Imm32(isberd.imm);
        } else {
            index = ir.IAdd(X(isberd.src_reg), ir.Imm32(isberd.imm));
        }

        switch (static_cast<u64>(isberd.mode.Value())) {
        case static_cast<u64>(Mode::Patch):
            result = ir.GetPatch(index.Patch());
            break;
        case static_cast<u64>(Mode::Prim):
            result = ir.GetAttribute(index.Attribute());
            break;
        case static_cast<u64>(Mode::Attr):
            result = ir.GetAttributeIndexed(index);
            break;
        }
        X(isberd.dest_reg, ir.BitCast<IR::U32>(result));
    }
    if (isberd.shift != Shift::Default) {
        IR::U32 result{};
        switch (static_cast<u64>(isberd.shift.Value())) {
        case static_cast<u64>(Shift::U16):
            result = ir.ShiftLeftLogical(result, static_cast<IR::U32>(ir.Imm16(1)));
            break;
        case static_cast<u64>(Shift::B32):
            result = ir.ShiftLeftLogical(result, ir.Imm32(1));
            break;
        }
        X(isberd.dest_reg, result);
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
