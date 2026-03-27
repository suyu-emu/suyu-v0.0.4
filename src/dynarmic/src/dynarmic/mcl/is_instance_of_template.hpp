// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: Copyright 2022 merryhime
// SPDX-License-Identifier: MIT

#pragma once

#include <cstddef>
#include <type_traits>

namespace mcl {

/// A metavalue (of type VT and value v).
template<class VT, VT v> using value = std::integral_constant<VT, v>;
/// A metavalue of type size_t (and value v).
template<size_t v> using size_value = value<size_t, v>;
/// A metavalue of type bool (and value v). (Aliases to std::bool_constant.)
template<bool v> using bool_value = value<bool, v>;
/// true metavalue (Aliases to std::true_type).
using true_type = bool_value<true>;
/// false metavalue (Aliases to std::false_type).
using false_type = bool_value<false>;

/// Is type T an instance of template class C?
template<template<class...> class, class>
struct is_instance_of_template : false_type {};

template<template<class...> class C, class... As>
struct is_instance_of_template<C, C<As...>> : true_type {};

/// Is type T an instance of template class C?
template<template<class...> class C, class T>
constexpr bool is_instance_of_template_v = is_instance_of_template<C, T>::value;

}  // namespace mcl
