// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <optional>
#include <set>

#include "video_core/engines/shader_bytecode.h"
#include "video_core/shader/ast.h"
#include "video_core/shader/compiler_settings.h"
#include "video_core/shader/shader_ir.h"

namespace VideoCommon::Shader {

using Tegra::Shader::ConditionCode;
using Tegra::Shader::Pred;

constexpr s32 exit_branch = -1;

struct Condition {
    Pred predicate{Pred::UnusedIndex};
    ConditionCode cc{ConditionCode::T};

    bool IsUnconditional() const {
        return predicate == Pred::UnusedIndex && cc == ConditionCode::T;
    }

    bool operator==(const Condition& other) const {
        return std::tie(predicate, cc) == std::tie(other.predicate, other.cc);
    }

    bool operator!=(const Condition& other) const {
        return !operator==(other);
    }
};

struct ShaderBlock {
    struct Branch {
        Condition cond{};
        bool kills{};
        s32 address{};

        bool operator==(const Branch& b) const {
            return std::tie(cond, kills, address) == std::tie(b.cond, b.kills, b.address);
        }

        bool operator!=(const Branch& b) const {
            return !operator==(b);
        }
    };

    u32 start{};
    u32 end{};
    bool ignore_branch{};
    Branch branch{};

    bool operator==(const ShaderBlock& sb) const {
        return std::tie(start, end, ignore_branch, branch) ==
               std::tie(sb.start, sb.end, sb.ignore_branch, sb.branch);
    }

    bool operator!=(const ShaderBlock& sb) const {
        return !operator==(sb);
    }
};

struct ShaderCharacteristics {
    std::list<ShaderBlock> blocks{};
    std::set<u32> labels{};
    u32 start{};
    u32 end{};
    ASTManager manager{true, true};
    CompilerSettings settings{};
};

std::unique_ptr<ShaderCharacteristics> ScanFlow(const ProgramCode& program_code, u32 program_size,
                                                u32 start_address,
                                                const CompilerSettings& settings);

} // namespace VideoCommon::Shader
