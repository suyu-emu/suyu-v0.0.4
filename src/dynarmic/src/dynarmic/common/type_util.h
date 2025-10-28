// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2018 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#pragma once

#include <mcl/type_traits/function_info.hpp>

namespace Dynarmic::Common {

/// Cast a lambda into an equivalent function pointer.
template<class Function>
inline auto FptrCast(Function f) noexcept {
    return static_cast<mcl::equivalent_function_type<Function>*>(f);
}

namespace Detail {
template<std::size_t size> struct IntegerOfSize {};
template<> struct IntegerOfSize<8> { using U = std::uint8_t; using S = std::int8_t; };
template<> struct IntegerOfSize<16> { using U = std::uint16_t; using S = std::int16_t; };
template<> struct IntegerOfSize<32> { using U = std::uint32_t; using S = std::int32_t; };
template<> struct IntegerOfSize<64> { using U = std::uint64_t; using S = std::int64_t; };
}
template<size_t N> using UnsignedIntegerN = typename Detail::IntegerOfSize<N>::U;
template<size_t N> using SignedIntegerN = typename Detail::IntegerOfSize<N>::S;

}  // namespace Dynarmic::Common
