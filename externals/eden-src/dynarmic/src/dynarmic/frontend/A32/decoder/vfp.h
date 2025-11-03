// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2032 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <vector>

#include "dynarmic/common/common_types.h"

#include "dynarmic/frontend/decoder/decoder_detail.h"
#include "dynarmic/frontend/decoder/matcher.h"

namespace Dynarmic::A32 {

template<typename Visitor>
using VFPMatcher = Decoder::Matcher<Visitor, u32>;

template<typename V>
std::optional<std::reference_wrapper<const VFPMatcher<V>>> DecodeVFP(u32 instruction) {
    using Table = std::vector<VFPMatcher<V>>;
    alignas(64) static const struct Tables {
        Table unconditional;
        Table conditional;
    } tables = []() {
        Table list = {
#define INST(fn, name, bitstring) DYNARMIC_DECODER_GET_MATCHER(VFPMatcher, fn, name, Decoder::detail::StringToArray<32>(bitstring)),
#include "./vfp.inc"
#undef INST
        };
        auto const it = std::stable_partition(list.begin(), list.end(), [&](const auto& matcher) {
            return (matcher.GetMask() & 0xF0000000) == 0xF0000000;
        });
        return Tables{
            Table{list.begin(), it},
            Table{it, list.end()},
        };
    }();
    const bool is_unconditional = (instruction & 0xF0000000) == 0xF0000000;
    const Table& table = is_unconditional ? tables.unconditional : tables.conditional;
    auto iter = std::find_if(table.begin(), table.end(), [instruction](const auto& matcher) {
        return matcher.Matches(instruction);
    });
    return iter != table.end() ? std::optional<std::reference_wrapper<const VFPMatcher<V>>>(*iter) : std::nullopt;
}

template<typename V>
std::optional<std::string_view> GetNameVFP(u32 inst) noexcept {
    std::vector<std::pair<std::string_view, VFPMatcher<V>>> list = {
#define INST(fn, name, bitstring) { name, DYNARMIC_DECODER_GET_MATCHER(VFPMatcher, fn, name, Decoder::detail::StringToArray<32>(bitstring)) },
#include "./vfp.inc"
#undef INST
    };
    auto const iter = std::find_if(list.cbegin(), list.cend(), [inst](auto const& m) {
        return m.second.Matches(inst);
    });
    return iter != list.cend() ? std::optional{iter->first} : std::nullopt;
}

}  // namespace Dynarmic::A32
