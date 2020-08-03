// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

namespace Common {

#include <type_traits>

// Check if type is like an STL container
template <typename T>
concept IsSTLContainer = requires(T t) {
    typename T::value_type;
    typename T::iterator;
    typename T::const_iterator;
    // TODO(ogniK): Replace below is std::same_as<void> when MSVC supports it.
    t.begin();
    t.end();
    t.cbegin();
    t.cend();
    t.data();
    t.size();
};

// Check if type T is derived from T2
template <typename T, typename T2>
concept IsBaseOf = requires {
    std::is_base_of_v<T, T2>;
};

} // namespace Common
