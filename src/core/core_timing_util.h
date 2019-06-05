// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include "common/common_types.h"

namespace Core::Timing {

// The below clock rate is based on Switch's clockspeed being widely known as 1.020GHz
// The exact value used is of course unverified.
constexpr u64 BASE_CLOCK_RATE = 1019215872; // Switch clock speed is 1020MHz un/docked
constexpr u64 CNTFREQ = 19200000;           // Value from fusee.

s64 msToCycles(std::chrono::milliseconds ms);
s64 usToCycles(std::chrono::microseconds us);
s64 nsToCycles(std::chrono::nanoseconds ns);

inline std::chrono::milliseconds CyclesToMs(s64 cycles) {
    return std::chrono::milliseconds(cycles * 1000 / BASE_CLOCK_RATE);
}

inline std::chrono::nanoseconds CyclesToNs(s64 cycles) {
    return std::chrono::nanoseconds(cycles * 1000000000 / BASE_CLOCK_RATE);
}

inline std::chrono::microseconds CyclesToUs(s64 cycles) {
    return std::chrono::microseconds(cycles * 1000000 / BASE_CLOCK_RATE);
}

u64 CpuCyclesToClockCycles(u64 ticks);

} // namespace Core::Timing
