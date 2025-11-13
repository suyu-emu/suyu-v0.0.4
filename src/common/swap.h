// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2012 PPSSPP Project
// SPDX-FileCopyrightText: 2012 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Official git repository and contact information can be found at
// https://github.com/hrydgard/ppsspp and http://www.ppsspp.org/.

#pragma once

#if defined(_MSC_VER)
#include <cstdlib>
#endif
#include <bit>
#include <cstring>
#include <type_traits>
#include "common/common_types.h"

namespace Common {

#if defined(__Bitrig__) || defined(__OpenBSD__)
// We'll redefine swap16, swap32, swap64 as inline functions
// but OpenBSD is like "wow I bring my own stuff"
// It would be nice if we could use them without C++ namespace shenanigans
// But alas :)
#undef swap16
#undef swap32
#undef swap64
#endif

#if defined(_MSC_VER)
[[nodiscard]] inline u16 swap16(u16 data) noexcept {
    return _byteswap_ushort(data);
}
[[nodiscard]] inline u32 swap32(u32 data) noexcept {
    return _byteswap_ulong(data);
}
[[nodiscard]] inline u64 swap64(u64 data) noexcept {
    return _byteswap_uint64(data);
}
#elif defined(__clang__) || defined(__GNUC__)
[[nodiscard]] inline u16 swap16(u16 data) noexcept {
    return __builtin_bswap16(data);
}
[[nodiscard]] inline u32 swap32(u32 data) noexcept {
    return __builtin_bswap32(data);
}
[[nodiscard]] inline u64 swap64(u64 data) noexcept {
    return __builtin_bswap64(data);
}
#else
// Generic implementation - compiler will optimise these into their respective
// __builtin_byteswapXX() and such; if not, the compiler is stupid and we probably
// have bigger problems to worry about :)
[[nodiscard]] inline u16 swap16(u16 data) noexcept {
    return (data >> 8) | (data << 8);
}
[[nodiscard]] inline u32 swap32(u32 data) noexcept {
    return ((data & 0xFF000000U) >> 24) | ((data & 0x00FF0000U) >> 8) |
           ((data & 0x0000FF00U) << 8) | ((data & 0x000000FFU) << 24);
}
[[nodiscard]] inline u64 swap64(u64 data) noexcept {
    return ((data & 0xFF00000000000000ULL) >> 56) | ((data & 0x00FF000000000000ULL) >> 40) |
           ((data & 0x0000FF0000000000ULL) >> 24) | ((data & 0x000000FF00000000ULL) >> 8) |
           ((data & 0x00000000FF000000ULL) << 8) | ((data & 0x0000000000FF0000ULL) << 24) |
           ((data & 0x000000000000FF00ULL) << 40) | ((data & 0x00000000000000FFULL) << 56);
}
#endif

[[nodiscard]] inline float swapf(float f) noexcept {
    static_assert(sizeof(u32) == sizeof(float), "float must be the same size as uint32_t.");
    u32 value;
    std::memcpy(&value, &f, sizeof(u32));
    value = swap32(value);
    std::memcpy(&f, &value, sizeof(u32));
    return f;
}

[[nodiscard]] inline double swapd(double f) noexcept {
    static_assert(sizeof(u64) == sizeof(double), "double must be the same size as uint64_t.");
    u64 value;
    std::memcpy(&value, &f, sizeof(u64));
    value = swap64(value);
    std::memcpy(&f, &value, sizeof(u64));
    return f;
}

} // Namespace Common

template <typename T, typename F>
struct SwapStructT {
    using SwappedT = SwapStructT;

protected:
    T value;

    static T swap(T v) {
        return F::swap(v);
    }

public:
    T swap() const {
        return swap(value);
    }
    SwapStructT() = default;
    SwapStructT(const T& v) : value(swap(v)) {}

    template <typename S>
    SwappedT& operator=(const S& source) {
        value = swap(T(source));
        return *this;
    }

    operator s8() const {
        return s8(swap());
    }
    operator u8() const {
        return u8(swap());
    }
    operator s16() const {
        return s16(swap());
    }
    operator u16() const {
        return u16(swap());
    }
    operator s32() const {
        return s32(swap());
    }
    operator u32() const {
        return u32(swap());
    }
    operator s64() const {
        return s64(swap());
    }
    operator u64() const {
        return u64(swap());
    }
    operator float() const {
        return float(swap());
    }
    operator double() const {
        return double(swap());
    }

    // +v
    SwappedT operator+() const {
        return +swap();
    }
    // -v
    SwappedT operator-() const {
        return -swap();
    }

    // v / 5
    SwappedT operator/(const SwappedT& i) const {
        return swap() / i.swap();
    }
    template <typename S>
    SwappedT operator/(const S& i) const {
        return swap() / i;
    }

    // v * 5
    SwappedT operator*(const SwappedT& i) const {
        return swap() * i.swap();
    }
    template <typename S>
    SwappedT operator*(const S& i) const {
        return swap() * i;
    }

    // v + 5
    SwappedT operator+(const SwappedT& i) const {
        return swap() + i.swap();
    }
    template <typename S>
    SwappedT operator+(const S& i) const {
        return swap() + T(i);
    }
    // v - 5
    SwappedT operator-(const SwappedT& i) const {
        return swap() - i.swap();
    }
    template <typename S>
    SwappedT operator-(const S& i) const {
        return swap() - T(i);
    }

    // v += 5
    SwappedT& operator+=(const SwappedT& i) {
        value = swap(swap() + i.swap());
        return *this;
    }
    template <typename S>
    SwappedT& operator+=(const S& i) {
        value = swap(swap() + T(i));
        return *this;
    }
    // v -= 5
    SwappedT& operator-=(const SwappedT& i) {
        value = swap(swap() - i.swap());
        return *this;
    }
    template <typename S>
    SwappedT& operator-=(const S& i) {
        value = swap(swap() - T(i));
        return *this;
    }

    // ++v
    SwappedT& operator++() {
        value = swap(swap() + 1);
        return *this;
    }
    // --v
    SwappedT& operator--() {
        value = swap(swap() - 1);
        return *this;
    }

    // v++
    SwappedT operator++(int) {
        SwappedT old = *this;
        value = swap(swap() + 1);
        return old;
    }
    // v--
    SwappedT operator--(int) {
        SwappedT old = *this;
        value = swap(swap() - 1);
        return old;
    }
    // Comparison
    // v == i
    bool operator==(const SwappedT& i) const {
        return swap() == i.swap();
    }
    template <typename S>
    bool operator==(const S& i) const {
        return swap() == i;
    }

    // v != i
    bool operator!=(const SwappedT& i) const {
        return swap() != i.swap();
    }
    template <typename S>
    bool operator!=(const S& i) const {
        return swap() != i;
    }

    // v > i
    bool operator>(const SwappedT& i) const {
        return swap() > i.swap();
    }
    template <typename S>
    bool operator>(const S& i) const {
        return swap() > i;
    }

    // v < i
    bool operator<(const SwappedT& i) const {
        return swap() < i.swap();
    }
    template <typename S>
    bool operator<(const S& i) const {
        return swap() < i;
    }

    // v >= i
    bool operator>=(const SwappedT& i) const {
        return swap() >= i.swap();
    }
    template <typename S>
    bool operator>=(const S& i) const {
        return swap() >= i;
    }

    // v <= i
    bool operator<=(const SwappedT& i) const {
        return swap() <= i.swap();
    }
    template <typename S>
    bool operator<=(const S& i) const {
        return swap() <= i;
    }

    // logical
    SwappedT operator!() const {
        return !swap();
    }

    // bitmath
    SwappedT operator~() const {
        return ~swap();
    }

    SwappedT operator&(const SwappedT& b) const {
        return swap() & b.swap();
    }
    template <typename S>
    SwappedT operator&(const S& b) const {
        return swap() & b;
    }
    SwappedT& operator&=(const SwappedT& b) {
        value = swap(swap() & b.swap());
        return *this;
    }
    template <typename S>
    SwappedT& operator&=(const S b) {
        value = swap(swap() & b);
        return *this;
    }

    SwappedT operator|(const SwappedT& b) const {
        return swap() | b.swap();
    }
    template <typename S>
    SwappedT operator|(const S& b) const {
        return swap() | b;
    }
    SwappedT& operator|=(const SwappedT& b) {
        value = swap(swap() | b.swap());
        return *this;
    }
    template <typename S>
    SwappedT& operator|=(const S& b) {
        value = swap(swap() | b);
        return *this;
    }

    SwappedT operator^(const SwappedT& b) const {
        return swap() ^ b.swap();
    }
    template <typename S>
    SwappedT operator^(const S& b) const {
        return swap() ^ b;
    }
    SwappedT& operator^=(const SwappedT& b) {
        value = swap(swap() ^ b.swap());
        return *this;
    }
    template <typename S>
    SwappedT& operator^=(const S& b) {
        value = swap(swap() ^ b);
        return *this;
    }

    template <typename S>
    SwappedT operator<<(const S& b) const {
        return swap() << b;
    }
    template <typename S>
    SwappedT& operator<<=(const S& b) const {
        value = swap(swap() << b);
        return *this;
    }

    template <typename S>
    SwappedT operator>>(const S& b) const {
        return swap() >> b;
    }
    template <typename S>
    SwappedT& operator>>=(const S& b) const {
        value = swap(swap() >> b);
        return *this;
    }

    // Member
    /** todo **/

    // Arithmetic
    template <typename S, typename T2, typename F2>
    friend S operator+(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend S operator-(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend S operator/(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend S operator*(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend S operator%(const S& p, const SwappedT v);

    // Arithmetic + assignments
    template <typename S, typename T2, typename F2>
    friend S operator+=(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend S operator-=(const S& p, const SwappedT v);

    // Bitmath
    template <typename S, typename T2, typename F2>
    friend S operator&(const S& p, const SwappedT v);

    // Comparison
    template <typename S, typename T2, typename F2>
    friend bool operator<(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend bool operator>(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend bool operator<=(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend bool operator>=(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend bool operator!=(const S& p, const SwappedT v);

    template <typename S, typename T2, typename F2>
    friend bool operator==(const S& p, const SwappedT v);
};

// Arithmetic
template <typename S, typename T, typename F>
S operator+(const S& i, const SwapStructT<T, F> v) {
    return i + v.swap();
}

template <typename S, typename T, typename F>
S operator-(const S& i, const SwapStructT<T, F> v) {
    return i - v.swap();
}

template <typename S, typename T, typename F>
S operator/(const S& i, const SwapStructT<T, F> v) {
    return i / v.swap();
}

template <typename S, typename T, typename F>
S operator*(const S& i, const SwapStructT<T, F> v) {
    return i * v.swap();
}

template <typename S, typename T, typename F>
S operator%(const S& i, const SwapStructT<T, F> v) {
    return i % v.swap();
}

// Arithmetic + assignments
template <typename S, typename T, typename F>
S& operator+=(S& i, const SwapStructT<T, F> v) {
    i += v.swap();
    return i;
}

template <typename S, typename T, typename F>
S& operator-=(S& i, const SwapStructT<T, F> v) {
    i -= v.swap();
    return i;
}

// Logical
template <typename S, typename T, typename F>
S operator&(const S& i, const SwapStructT<T, F> v) {
    return i & v.swap();
}

// Comparison
template <typename S, typename T, typename F>
bool operator<(const S& p, const SwapStructT<T, F> v) {
    return p < v.swap();
}
template <typename S, typename T, typename F>
bool operator>(const S& p, const SwapStructT<T, F> v) {
    return p > v.swap();
}
template <typename S, typename T, typename F>
bool operator<=(const S& p, const SwapStructT<T, F> v) {
    return p <= v.swap();
}
template <typename S, typename T, typename F>
bool operator>=(const S& p, const SwapStructT<T, F> v) {
    return p >= v.swap();
}
template <typename S, typename T, typename F>
bool operator!=(const S& p, const SwapStructT<T, F> v) {
    return p != v.swap();
}
template <typename S, typename T, typename F>
bool operator==(const S& p, const SwapStructT<T, F> v) {
    return p == v.swap();
}

template <typename T>
struct Swap64T {
    static T swap(T x) {
        return T(Common::swap64(x));
    }
};

template <typename T>
struct Swap32T {
    static T swap(T x) {
        return T(Common::swap32(x));
    }
};

template <typename T>
struct Swap16T {
    static T swap(T x) {
        return T(Common::swap16(x));
    }
};

template <typename T>
struct SwapFloatT {
    static T swap(T x) {
        return T(Common::swapf(x));
    }
};

template <typename T>
struct SwapDoubleT {
    static T swap(T x) {
        return T(Common::swapd(x));
    }
};

template <typename T>
struct SwapEnumT {
    static_assert(std::is_enum_v<T>);
    using base = std::underlying_type_t<T>;

public:
    SwapEnumT() = default;
    SwapEnumT(const T& v) : value(swap(v)) {}

    SwapEnumT& operator=(const T& v) {
        value = swap(v);
        return *this;
    }

    operator T() const {
        return swap(value);
    }

    explicit operator base() const {
        return base(swap(value));
    }

protected:
    T value{};
    using swap_t = std::conditional_t<
        std::is_same_v<base, u16>, Swap16T<u16>, std::conditional_t<
        std::is_same_v<base, s16>, Swap16T<s16>, std::conditional_t<
        std::is_same_v<base, u32>, Swap32T<u32>, std::conditional_t<
        std::is_same_v<base, s32>, Swap32T<s32>, std::conditional_t<
        std::is_same_v<base, u64>, Swap64T<u64>, std::conditional_t<
        std::is_same_v<base, s64>, Swap64T<s64>, void>>>>>>;
    static T swap(T x) {
        return T(swap_t::swap(base(x)));
    }
};

struct SwapTag {}; // Use the different endianness from the system
struct KeepTag {}; // Use the same endianness as the system

template <typename T, typename Tag>
struct AddEndian;

// KeepTag specializations

template <typename T>
struct AddEndian<T, KeepTag> {
    using type = T;
};

// SwapTag specializations

template <>
struct AddEndian<u8, SwapTag> {
    using type = u8;
};

template <>
struct AddEndian<u16, SwapTag> {
    using type = SwapStructT<u16, Swap16T<u16>>;
};

template <>
struct AddEndian<u32, SwapTag> {
    using type = SwapStructT<u32, Swap32T<u32>>;
};

template <>
struct AddEndian<u64, SwapTag> {
    using type = SwapStructT<u64, Swap64T<u64>>;
};

template <>
struct AddEndian<s8, SwapTag> {
    using type = s8;
};

template <>
struct AddEndian<s16, SwapTag> {
    using type = SwapStructT<s16, Swap16T<s16>>;
};

template <>
struct AddEndian<s32, SwapTag> {
    using type = SwapStructT<s32, Swap32T<s32>>;
};

template <>
struct AddEndian<s64, SwapTag> {
    using type = SwapStructT<s64, Swap64T<s64>>;
};

template <>
struct AddEndian<float, SwapTag> {
    using type = SwapStructT<float, SwapFloatT<float>>;
};

template <>
struct AddEndian<double, SwapTag> {
    using type = SwapStructT<double, SwapDoubleT<double>>;
};

template <typename T>
struct AddEndian<T, SwapTag> {
    static_assert(std::is_enum_v<T>);
    using type = SwapEnumT<T>;
};

// Alias LETag/BETag as KeepTag/SwapTag depending on the system
using LETag = std::conditional_t<std::endian::native == std::endian::little, KeepTag, SwapTag>;
using BETag = std::conditional_t<std::endian::native == std::endian::big, KeepTag, SwapTag>;

// Aliases for LE types
using u16_le = AddEndian<u16, LETag>::type;
using u32_le = AddEndian<u32, LETag>::type;
using u64_le = AddEndian<u64, LETag>::type;

using s16_le = AddEndian<s16, LETag>::type;
using s32_le = AddEndian<s32, LETag>::type;
using s64_le = AddEndian<s64, LETag>::type;

template <typename T>
using enum_le = std::enable_if_t<std::is_enum_v<T>, typename AddEndian<T, LETag>::type>;

using float_le = AddEndian<float, LETag>::type;
using double_le = AddEndian<double, LETag>::type;

// Aliases for BE types
using u16_be = AddEndian<u16, BETag>::type;
using u32_be = AddEndian<u32, BETag>::type;
using u64_be = AddEndian<u64, BETag>::type;

using s16_be = AddEndian<s16, BETag>::type;
using s32_be = AddEndian<s32, BETag>::type;
using s64_be = AddEndian<s64, BETag>::type;

template <typename T>
using enum_be = std::enable_if_t<std::is_enum_v<T>, typename AddEndian<T, BETag>::type>;

using float_be = AddEndian<float, BETag>::type;
using double_be = AddEndian<double, BETag>::type;
