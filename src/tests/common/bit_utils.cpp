// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch.hpp>
#include <math.h>
#include "common/bit_util.h"

namespace Common {

inline u32 CTZ32(u32 value) {
  u32 count = 0;
  while (((value >> count) & 0xf) == 0 && count < 32)
    count += 4;
  while (((value >> count) & 1) == 0 && count < 32)
    count++;
  return count;
}

inline u64 CTZ64(u64 value) {
  u64 count = 0;
  while (((value >> count) & 0xf) == 0 && count < 64)
    count += 4;
  while (((value >> count) & 1) == 0 && count < 64)
    count++;
  return count;
}


TEST_CASE("BitUtils", "[common]") {
    REQUIRE(Common::CountTrailingZeroes32(0) == CTZ32(0));
    REQUIRE(Common::CountTrailingZeroes64(0) == CTZ64(0));
    REQUIRE(Common::CountTrailingZeroes32(9) == CTZ32(9));
    REQUIRE(Common::CountTrailingZeroes32(8) == CTZ32(8));
    REQUIRE(Common::CountTrailingZeroes32(0x801000) == CTZ32(0x801000));
    REQUIRE(Common::CountTrailingZeroes64(9) == CTZ64(9));
    REQUIRE(Common::CountTrailingZeroes64(8) == CTZ64(8));
    REQUIRE(Common::CountTrailingZeroes64(0x801000) == CTZ64(0x801000));
    REQUIRE(Common::CountTrailingZeroes64(0x801000000000UL) == CTZ64(0x801000000000UL));
}

} // namespace Common
