// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Core {

union EmuThreadHandle {
    u64 raw;
    struct {
        u32 host_handle;
        u32 guest_handle;
    };
};

namespace Hardware {

// The below clock rate is based on Switch's clockspeed being widely known as 1.020GHz
// The exact value used is of course unverified.
constexpr u64 BASE_CLOCK_RATE = 1019215872; // Switch cpu frequency is 1020MHz un/docked
constexpr u64 CNTFREQ = 19200000;           // Switch's hardware clock speed
constexpr u32 NUM_CPU_CORES = 4;            // Number of CPU Cores
} // namespace Hardware

} // namespace Core
