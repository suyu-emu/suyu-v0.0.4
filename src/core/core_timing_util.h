// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include "common/common_types.h"
#include "core/hardware_properties.h"

namespace Core::Timing {

s64 msToCycles(std::chrono::milliseconds ms);
s64 usToCycles(std::chrono::microseconds us);
s64 nsToCycles(std::chrono::nanoseconds ns);
u64 msToClockCycles(std::chrono::milliseconds ns);
u64 usToClockCycles(std::chrono::microseconds ns);
u64 nsToClockCycles(std::chrono::nanoseconds ns);

inline std::chrono::milliseconds CyclesToMs(s64 cycles) {
    return std::chrono::milliseconds(cycles * 1000 / Hardware::BASE_CLOCK_RATE);
}

inline std::chrono::nanoseconds CyclesToNs(s64 cycles) {
    return std::chrono::nanoseconds(cycles * 1000000000 / Hardware::BASE_CLOCK_RATE);
}

inline std::chrono::microseconds CyclesToUs(s64 cycles) {
    return std::chrono::microseconds(cycles * 1000000 / Hardware::BASE_CLOCK_RATE);
}

u64 CpuCyclesToClockCycles(u64 ticks);

} // namespace Core::Timing
