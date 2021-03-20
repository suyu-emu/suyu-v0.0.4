// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

enum : u64 {
    Size_1_MB = 0x100000ULL,
    Size_2_MB = 2ULL * Size_1_MB,
    Size_4_MB = 4ULL * Size_1_MB,
    Size_14_MB = 14ULL * Size_1_MB,
    Size_32_MB = 32ULL * Size_1_MB,
    Size_128_MB = 128ULL * Size_1_MB,
    Size_1_GB = 0x40000000ULL,
    Size_2_GB = 2ULL * Size_1_GB,
    Size_4_GB = 4ULL * Size_1_GB,
    Size_6_GB = 6ULL * Size_1_GB,
    Size_64_GB = 64ULL * Size_1_GB,
    Size_512_GB = 512ULL * Size_1_GB,
    Invalid = std::numeric_limits<u64>::max(),
};
