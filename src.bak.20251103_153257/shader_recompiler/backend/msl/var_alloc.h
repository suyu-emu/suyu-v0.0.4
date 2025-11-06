// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <bitset>
#include <string>
#include <vector>

#include "common/bit_field.h"
#include "common/common_types.h"

namespace Shader::IR {
class Inst;
class Value;
enum class Type;
} // namespace Shader::IR

namespace Shader::Backend::MSL {
enum class MslVarType : u32 {
    U1,
    F16x2,
    U32,
    F32,
    U64,
    F64,
    U32x2,
    F32x2,
    U32x3,
    F32x3,
    U32x4,
    F32x4,
    PrecF32,
    PrecF64,
    Void,
};

struct Id {
    union {
        u32 raw;
        BitField<0, 1, u32> is_valid;
        BitField<1, 4, MslVarType> type;
        BitField<6, 26, u32> index;
    };

    bool operator==(Id rhs) const noexcept {
        return raw == rhs.raw;
    }
    bool operator!=(Id rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(sizeof(Id) == sizeof(u32));

class VarAlloc {
public:
    struct UseTracker {
        bool uses_temp{};
        size_t num_used{};
        std::vector<bool> var_use;
    };

    /// Used for explicit usages of variables, may revert to temporaries
    std::string Define(IR::Inst& inst, MslVarType type);
    std::string Define(IR::Inst& inst, IR::Type type);

    /// Used to assign variables used by the IR. May return a blank string if
    /// the instruction's result is unused in the IR.
    std::string AddDefine(IR::Inst& inst, MslVarType type);
    std::string PhiDefine(IR::Inst& inst, IR::Type type);

    std::string Consume(const IR::Value& value);
    std::string ConsumeInst(IR::Inst& inst);

    std::string GetMslType(MslVarType type) const;
    std::string GetMslType(IR::Type type) const;

    const UseTracker& GetUseTracker(MslVarType type) const;
    std::string Representation(u32 index, MslVarType type) const;

private:
    MslVarType RegType(IR::Type type) const;
    Id Alloc(MslVarType type);
    void Free(Id id);
    UseTracker& GetUseTracker(MslVarType type);
    std::string Representation(Id id) const;

    UseTracker var_bool{};
    UseTracker var_f16x2{};
    UseTracker var_u32{};
    UseTracker var_u32x2{};
    UseTracker var_u32x3{};
    UseTracker var_u32x4{};
    UseTracker var_f32{};
    UseTracker var_f32x2{};
    UseTracker var_f32x3{};
    UseTracker var_f32x4{};
    UseTracker var_u64{};
    UseTracker var_f64{};
    UseTracker var_precf32{};
    UseTracker var_precf64{};
};

} // namespace Shader::Backend::MSL
