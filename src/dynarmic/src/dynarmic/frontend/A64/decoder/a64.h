// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "dynarmic/mcl/bit.hpp"
#include "dynarmic/common/common_types.h"

#include "dynarmic/frontend/decoder/decoder_detail.h"
#include "dynarmic/frontend/decoder/matcher.h"

namespace Dynarmic::A64 {

template<typename Visitor>
using Matcher = Decoder::Matcher<Visitor, u32>;

template<typename Visitor>
using DecodeTable = std::array<std::vector<Matcher<Visitor>>, 0x1000>;

namespace detail {
inline size_t ToFastLookupIndex(u32 instruction) {
    return ((instruction >> 10) & 0x00F) | ((instruction >> 18) & 0xFF0);
}
}  // namespace detail

template<typename V>
inline DecodeTable<V> GetDecodeTable() {
    DecodeTable<V> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        // PLEASE HEAP ELLIDE
        for (auto const& e : std::vector<Matcher<V>>{
#define INST(fn, name, bitstring) DYNARMIC_DECODER_GET_MATCHER(Matcher, fn, name, Decoder::detail::StringToArray<32>(bitstring)),
#include "./a64.inc"
#undef INST
        }) {
            const auto expect = detail::ToFastLookupIndex(e.GetExpected());
            const auto mask = detail::ToFastLookupIndex(e.GetMask());
            if ((i & mask) == expect)
                table[i].push_back(e);
        }
    }
    return table;
}

/// In practice it must always suceed, otherwise something else unrelated would have gone awry
template<typename V>
inline std::optional<std::reference_wrapper<const Matcher<V>>> Decode(u32 instruction) {
    alignas(64) static const auto table = GetDecodeTable<V>();
    const auto& subtable = table[detail::ToFastLookupIndex(instruction)];
    auto iter = std::find_if(subtable.begin(), subtable.end(), [instruction](const auto& matcher) {
        return matcher.Matches(instruction);
    });
    return iter != subtable.end()
        ? std::optional{ std::reference_wrapper<const Matcher<V>>(*iter) }
        : std::nullopt;
}

template<typename V>
inline std::optional<std::string_view> GetName(u32 inst) noexcept {
    std::vector<std::pair<std::string_view, Matcher<V>>> list = {
#define INST(fn, name, bitstring) { name, DYNARMIC_DECODER_GET_MATCHER(Matcher, fn, name, Decoder::detail::StringToArray<32>(bitstring)) },
#include "./a64.inc"
#undef INST
    };
    auto const iter = std::find_if(list.cbegin(), list.cend(), [inst](auto const& m) {
        return m.second.Matches(inst);
    });
    return iter != list.cend() ? std::optional{iter->first} : std::nullopt;
}

}  // namespace Dynarmic::A64
