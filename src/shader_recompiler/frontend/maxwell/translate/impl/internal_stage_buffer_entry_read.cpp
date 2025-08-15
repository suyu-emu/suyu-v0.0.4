// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/bit_field.h"
#include "common/common_types.h"
#include "shader_recompiler/frontend/maxwell/translate/impl/impl.h"

namespace Shader::Maxwell {

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
        BitField<33, 2, Isberd::Mode> mode;
        BitField<36, 4, Isberd::SZ> sz;
        BitField<47, 2, Isberd::Shift> shift;
    } const isberd{insn};

    auto address = compute_ISBERD_address(isberd.src_reg, isberd.src_reg_num, isberd.imm, isberd.skew);
    if (isberd.o != 0) {
        auto result = apply_ISBERD_size_read(address, isberd.sz.Value());
        X(isberd.dest_reg, apply_ISBERD_shift(result, isberd.shift.Value()));

        return;
    }

    if (isberd.mode != Isberd::Mode::Default) {
        IR::F32 result_f32{};
        switch (isberd.mode.Value()) {
        case Isberd::Mode::Patch:
            result_f32 = ir.GetPatch(address.Patch());
            break;
        case Isberd::Mode::Prim:
            result_f32 = ir.GetAttribute(address.Attribute());
            break;
        case Isberd::Mode::Attr:
            result_f32 = ir.GetAttributeIndexed(address);
            break;
        default:
            UNREACHABLE();
        }

        auto result_u32 = ir.BitCast<IR::U32>(result_f32);
        X(isberd.dest_reg, apply_ISBERD_shift(result_u32, isberd.shift.Value()));
        return;
    }

    if (isberd.skew != 0) {
        auto result = ir.IAdd(X(isberd.src_reg), ir.LaneId());
        X(isberd.dest_reg, result);

        return;
    }

    // Fallback if nothing else applies
    X(isberd.dest_reg, X(isberd.src_reg));
}

} // namespace Shader::Maxwell
