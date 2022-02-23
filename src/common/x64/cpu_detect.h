// Copyright 2013 Dolphin Emulator Project / 2015 Citra Emulator Project / 2022 Yuzu Emulator
// Project Project Licensed under GPLv2 or any later version Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Common {

enum class Manufacturer : u8 {
    Intel = 0,
    AMD = 1,
    Hygon = 2,
    Unknown = 3,
};

/// x86/x64 CPU capabilities that may be detected by this module
struct CPUCaps {
    Manufacturer manufacturer;
    char cpu_string[0x21];
    char brand_string[0x41];
    u32 base_frequency;
    u32 max_frequency;
    u32 bus_frequency;
    bool sse : 1;
    bool sse2 : 1;
    bool sse3 : 1;
    bool ssse3 : 1;
    bool sse4_1 : 1;
    bool sse4_2 : 1;
    bool lzcnt : 1;
    bool avx : 1;
    bool avx2 : 1;
    bool avx512 : 1;
    bool bmi1 : 1;
    bool bmi2 : 1;
    bool fma : 1;
    bool fma4 : 1;
    bool aes : 1;
    bool invariant_tsc : 1;
};

/**
 * Gets the supported capabilities of the host CPU
 * @return Reference to a CPUCaps struct with the detected host CPU capabilities
 */
const CPUCaps& GetCPUCaps();

} // namespace Common
