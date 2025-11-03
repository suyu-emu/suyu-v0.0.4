// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2016 MerryMage
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
using Thumb16Matcher = Decoder::Matcher<Visitor, u16>;

template<typename V>
std::optional<std::reference_wrapper<const Thumb16Matcher<V>>> DecodeThumb16(u16 instruction) {
    alignas(64) static const std::vector<Thumb16Matcher<V>> table = {
#define INST(fn, name, bitstring) DYNARMIC_DECODER_GET_MATCHER(Thumb16Matcher, fn, name, Decoder::detail::StringToArray<16>(bitstring)),
#include "./thumb16.inc"
#undef INST
    };
    auto iter = std::find_if(table.begin(), table.end(), [instruction](const auto& matcher) {
        return matcher.Matches(instruction);
    });
    return iter != table.end() ? std::optional<std::reference_wrapper<const Thumb16Matcher<V>>>(*iter) : std::nullopt;
}

template<typename V>
std::optional<std::string_view> GetNameThumb16(u32 inst) noexcept {
    std::vector<std::pair<std::string_view, Thumb16Matcher<V>>> list = {
#define INST(fn, name, bitstring) { name, DYNARMIC_DECODER_GET_MATCHER(Thumb16Matcher, fn, name, Decoder::detail::StringToArray<16>(bitstring)) },
#include "./thumb16.inc"
#undef INST
    };
    auto const iter = std::find_if(list.cbegin(), list.cend(), [inst](auto const& m) {
        return m.second.Matches(inst);
    });
    return iter != list.cend() ? std::optional{iter->first} : std::nullopt;
}

}  // namespace Dynarmic::A32
