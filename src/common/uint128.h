#include <array>
#include <cstdint>
#include <utility>
#include <cstring>
#include "common/common_types.h"

namespace Common {

#ifdef _MSC_VER
#include <intrin.h>

#pragma intrinsic(_umul128)
#endif

inline u128 umul128(u64 a, u64 b) {
#ifdef _MSC_VER
u128 result;
result[0] = _umul128(a, b, &result[1]);
#else
unsigned __int128 tmp = a;
tmp *= b;
u128 result;
std::memcpy(&result, &tmp, sizeof(u128));
#endif
return result;
}

std::pair<u64, u64> udiv128(u128 dividend, u64 divisor);

} // namespace Common
