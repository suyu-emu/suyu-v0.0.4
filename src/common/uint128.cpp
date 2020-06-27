// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifdef _MSC_VER
#include <intrin.h>

#pragma intrinsic(_umul128)
#pragma intrinsic(_udiv128)
#endif
#include <cstring>
#include "common/uint128.h"

namespace Common {

#ifdef _MSC_VER

u64 MultiplyAndDivide64(u64 a, u64 b, u64 d) {
    u128 r{};
    r[0] = _umul128(a, b, &r[1]);
    u64 remainder;
#if _MSC_VER < 1923
    return udiv128(r[1], r[0], d, &remainder);
#else
    return _udiv128(r[1], r[0], d, &remainder);
#endif
}

#else

u64 MultiplyAndDivide64(u64 a, u64 b, u64 d) {
    const u64 diva = a / d;
    const u64 moda = a % d;
    const u64 divb = b / d;
    const u64 modb = b % d;
    return diva * b + moda * divb + moda * modb / d;
}

#endif

u128 Multiply64Into128(u64 a, u64 b) {
    u128 result;
#ifdef _MSC_VER
    result[0] = _umul128(a, b, &result[1]);
#else
    unsigned __int128 tmp = a;
    tmp *= b;
    std::memcpy(&result, &tmp, sizeof(u128));
#endif
    return result;
}

std::pair<u64, u64> Divide128On32(u128 dividend, u32 divisor) {
    u64 remainder = dividend[0] % divisor;
    u64 accum = dividend[0] / divisor;
    if (dividend[1] == 0)
        return {accum, remainder};
    // We ignore dividend[1] / divisor as that overflows
    const u64 first_segment = (dividend[1] % divisor) << 32;
    accum += (first_segment / divisor) << 32;
    const u64 second_segment = (first_segment % divisor) << 32;
    accum += (second_segment / divisor);
    remainder += second_segment % divisor;
    if (remainder >= divisor) {
        accum++;
        remainder -= divisor;
    }
    return {accum, remainder};
}

} // namespace Common
