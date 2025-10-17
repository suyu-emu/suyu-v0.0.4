// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <catch2/catch_test_macros.hpp>
#ifdef __AVX__
#include <immintrin.h>
#endif

// NetBSD apparently still needs these... ugh
#ifdef __cpp_lib_bit_cast
#include <bit>
template <typename To, typename From> constexpr inline To BitCast(const From& from) {
    return std::bit_cast<To>(from);
}
#else
template <typename To, typename From> constexpr inline To BitCast(const From& from) {
    return __builtin_bit_cast(To, from);
}
#endif

template<typename F>
void CheckedRun(F&& fn) {
#ifdef __AVX__
    __m256i xmm0 = _mm256_set_epi32(0, 0, 0, 0, 0, 0, 0, 0);
    __m256i xmm1 = _mm256_set_epi32(1, 1, 0, 0, 0, 0, 0, 1);
    __m256i xmm2 = _mm256_set_epi32(2, 2, 0, 0, 0, 0, 0, 2);
    __m256i xmm3 = _mm256_set_epi32(3, 3, 0, 0, 0, 0, 0, 3);
    __m256i xmm4 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 4);
    __m256i xmm5 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 5);
    __m256i xmm6 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 6);
    __m256i xmm7 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 7);
    __m256i xmm8 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 8);
    __m256i xmm9 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 9);
    __m256i xmm10 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 10);
    __m256i xmm11 = _mm256_set_epi32(4, 4, 0, 0, 0, 0, 0, 11);
    asm volatile(""
        : "+x"(xmm0), "+x"(xmm1), "+x"(xmm2), "+x"(xmm3)
        , "+x"(xmm4), "+x"(xmm5), "+x"(xmm6), "+x"(xmm7)
        , "+x"(xmm8), "+x"(xmm9), "+x"(xmm10), "+x"(xmm11)
        :
    );
    fn();
    asm volatile(""
        : "+x"(xmm0), "+x"(xmm1), "+x"(xmm2), "+x"(xmm3)
        , "+x"(xmm4), "+x"(xmm5), "+x"(xmm6), "+x"(xmm7)
        , "+x"(xmm8), "+x"(xmm9), "+x"(xmm10), "+x"(xmm11)
        :
    );
    CHECK(BitCast<std::uint64_t>(xmm0[0]) == 0);
    CHECK(BitCast<std::uint64_t>(xmm1[0]) == 1);
    CHECK(BitCast<std::uint64_t>(xmm2[0]) == 2);
    CHECK(BitCast<std::uint64_t>(xmm3[0]) == 3);
    CHECK(BitCast<std::uint64_t>(xmm4[0]) == 4);
    CHECK(BitCast<std::uint64_t>(xmm5[0]) == 5);
    CHECK(BitCast<std::uint64_t>(xmm6[0]) == 6);
    CHECK(BitCast<std::uint64_t>(xmm7[0]) == 7);
    CHECK(BitCast<std::uint64_t>(xmm8[0]) == 8);
    CHECK(BitCast<std::uint64_t>(xmm9[0]) == 9);
    CHECK(BitCast<std::uint64_t>(xmm10[0]) == 10);
    CHECK(BitCast<std::uint64_t>(xmm11[0]) == 11);
#else
    fn();
#endif
}
