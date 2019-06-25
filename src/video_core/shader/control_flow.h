#pragma once

#include <cstring>
#include <list>
#include <optional>
#include <vector>

#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::ConditionCode;
using Tegra::Shader::Pred;

constexpr s32 exit_branch = -1;

struct Condition {
    Pred predicate{Pred::UnusedIndex};
    ConditionCode cc{ConditionCode::T};

    bool IsUnconditional() const {
        return (predicate == Pred::UnusedIndex) && (cc == ConditionCode::T);
    }
};

struct ShaderBlock {
    ShaderBlock() {}
    ShaderBlock(const ShaderBlock& sb) = default;
    u32 start{};
    u32 end{};
    bool ignore_branch{};
    struct Branch {
        Condition cond{};
        bool kills{};
        s32 address{};
        bool operator==(const Branch& b) const {
            return std::memcmp(this, &b, sizeof(Branch)) == 0;
        }
    } branch;
    bool operator==(const ShaderBlock& sb) const {
        return std::memcmp(this, &sb, sizeof(ShaderBlock)) == 0;
    }
};

struct ShaderCharacteristics {
    std::list<ShaderBlock> blocks;
    bool decompilable{};
    u32 start;
    u32 end;
};

bool ScanFlow(const ProgramCode& program_code, u32 program_size, u32 start_address,
              ShaderCharacteristics& result_out);

} // namespace VideoCommon::Shader
