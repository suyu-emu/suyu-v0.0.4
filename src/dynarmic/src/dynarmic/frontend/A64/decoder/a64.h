// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
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

#include <mcl/bit/bit_count.hpp>
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
constexpr DecodeTable<V> GetDecodeTable() {
    std::vector<std::pair<const char*, Matcher<V>>> list = {
#define INST(fn, name, bitstring) { name, DYNARMIC_DECODER_GET_MATCHER(Matcher, fn, name, Decoder::detail::StringToArray<32>(bitstring)) },
#include "./a64.inc"
#undef INST
    };
    // If a matcher has more bits in its mask it is more specific, so it should come first.
    std::stable_sort(list.begin(), list.end(), [](const auto& a, const auto& b) {
        // If a matcher has more bits in its mask it is more specific, so it should come first.
        return mcl::bit::count_ones(a.second.GetMask()) > mcl::bit::count_ones(b.second.GetMask());
    });
    // Exceptions to the above rule of thumb.
    std::stable_partition(list.begin(), list.end(), [&](const auto& e) {
        return std::set<std::string>{
            "MOVI, MVNI, ORR, BIC (vector, immediate)",
            "FMOV (vector, immediate)",
            "Unallocated SIMD modified immediate",
        }.count(e.first) > 0;
    });
    DecodeTable<V> table{};
    for (size_t i = 0; i < table.size(); ++i) {
        for (auto const& e : list) {
            const auto expect = detail::ToFastLookupIndex(e.second.GetExpected());
            const auto mask = detail::ToFastLookupIndex(e.second.GetMask());
            if ((i & mask) == expect) {
                table[i].push_back(e.second);
            }
        }
    }
    return table;
}

template<typename V>
std::optional<std::reference_wrapper<const Matcher<V>>> Decode(u32 instruction) {
    alignas(64) static const auto table = GetDecodeTable<V>();
    const auto& subtable = table[detail::ToFastLookupIndex(instruction)];
    auto iter = std::find_if(subtable.begin(), subtable.end(), [instruction](const auto& matcher) {
        return matcher.Matches(instruction);
    });
    return iter != subtable.end() ? std::optional<std::reference_wrapper<const Matcher<V>>>(*iter) : std::nullopt;
}

template<typename V>
std::optional<std::string_view> GetName(u32 inst) noexcept {
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
