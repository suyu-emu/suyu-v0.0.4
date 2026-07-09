// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <iterator>
#include <utility>
#include <cstdlib> // for exit
#include "common/common_types.h"

#ifndef __cpp_lib_to_underlying
namespace std {
// https://en.cppreference.com/cpp/utility/to_underlying
template<class T>
    requires std::is_enum_v<T>
constexpr std::underlying_type_t<T> to_underlying(T e) noexcept {
    return std::underlying_type_t<T>(e);
}
}
#endif

/// Textually concatenates two tokens. The double-expansion is required by the C preprocessor.
#define CONCAT2(x, y) DO_CONCAT2(x, y)
#define DO_CONCAT2(x, y) x##y

/// Helper macros to insert unused bytes or words to properly align structs. These values will be
/// zero-initialized.
#define INSERT_PADDING_BYTES(num_bytes)                                                            \
    [[maybe_unused]] std::array<u8, num_bytes> CONCAT2(pad, __LINE__) {}
#define INSERT_PADDING_WORDS(num_words)                                                            \
    [[maybe_unused]] std::array<u32, num_words> CONCAT2(pad, __LINE__) {}

/// These are similar to the INSERT_PADDING_* macros but do not zero-initialize the contents.
/// This keeps the structure trivial to construct.
#define INSERT_PADDING_BYTES_NOINIT(num_bytes)                                                     \
    [[maybe_unused]] std::array<u8, num_bytes> CONCAT2(pad, __LINE__)
#define INSERT_PADDING_WORDS_NOINIT(num_words)                                                     \
    [[maybe_unused]] std::array<u32, num_words> CONCAT2(pad, __LINE__)

#ifdef _MSC_VER
#   define locale_t _locale_t // Locale Cross-Compatibility
#endif // _MSC_VER

#define DECLARE_ENUM_FLAG_OPERATORS(type)                                                          \
    [[nodiscard]] constexpr type operator|(type a, type b) noexcept {                              \
        return type(std::to_underlying(a) | std::to_underlying(b));                                \
    }                                                                                              \
    [[nodiscard]] constexpr type operator&(type a, type b) noexcept {                              \
        return type(std::to_underlying(a) & std::to_underlying(b));                                \
    }                                                                                              \
    [[nodiscard]] constexpr type operator^(type a, type b) noexcept {                              \
        return type(std::to_underlying(a) ^ std::to_underlying(b));                                \
    }                                                                                              \
    [[nodiscard]] constexpr type operator<<(type a, type b) noexcept {                             \
        return type(std::to_underlying(a) << std::to_underlying(b));                               \
    }                                                                                              \
    [[nodiscard]] constexpr type operator>>(type a, type b) noexcept {                             \
        return type(std::to_underlying(a) >> std::to_underlying(b));                               \
    }                                                                                              \
    constexpr type operator|=(type& a, type b) noexcept {                                          \
        return a = a | b;                                                                          \
    }                                                                                              \
    constexpr type operator&=(type& a, type b) noexcept {                                          \
        return a = a & b;                                                                          \
    }                                                                                              \
    constexpr type operator^=(type& a, type b) noexcept {                                          \
        return a = a ^ b;                                                                          \
    }                                                                                              \
    constexpr type operator<<=(type& a, type b) noexcept {                                         \
        return a = a << b;                                                                         \
    }                                                                                              \
    constexpr type operator>>=(type& a, type b) noexcept {                                         \
        return a = a >> b;                                                                         \
    }                                                                                              \
    [[nodiscard]] constexpr type operator~(type key) noexcept {                                    \
        return type(~std::to_underlying(key));                                                     \
    }                                                                                              \
    [[nodiscard]] constexpr bool True(type key) noexcept {                                         \
        return std::to_underlying(key) != 0;                                                       \
    }                                                                                              \
    [[nodiscard]] constexpr bool False(type key) noexcept {                                        \
        return std::to_underlying(key) == 0;                                                       \
    }

#define YUZU_NON_COPYABLE(cls)                                                                     \
    cls(const cls&) = delete;                                                                      \
    cls& operator=(const cls&) = delete

#define YUZU_NON_MOVEABLE(cls)                                                                     \
    cls(cls&&) = delete;                                                                           \
    cls& operator=(cls&&) = delete

namespace Common {

[[nodiscard]] constexpr u32 MakeMagic(char a, char b, char c, char d) {
    return u32(a) | u32(b) << 8 | u32(c) << 16 | u32(d) << 24;
}

[[nodiscard]] constexpr u64 MakeMagic(char a, char b, char c, char d, char e, char f, char g, char h) {
    return u64(a) << 0 | u64(b) << 8 | u64(c) << 16 | u64(d) << 24 | u64(e) << 32 | u64(f) << 40 | u64(g) << 48 | u64(h) << 56;
}

} // namespace Common
