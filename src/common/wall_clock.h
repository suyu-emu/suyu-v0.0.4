// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>

#include "common/common_types.h"

namespace Common {

class WallClock {
public:
    virtual std::chrono::nanoseconds GetTimeNS() = 0;
    virtual std::chrono::microseconds GetTimeUS() = 0;
    virtual std::chrono::milliseconds GetTimeMS() = 0;
    virtual u64 GetClockCycles() = 0;
    virtual u64 GetCPUCycles() = 0;

    /// Tells if the wall clock, uses the host CPU's hardware clock
    bool IsNative() const {
        return is_native;
    }

protected:
    WallClock(u64 emulated_cpu_frequency, u64 emulated_clock_frequency, bool is_native)
        : emulated_cpu_frequency{emulated_cpu_frequency},
          emulated_clock_frequency{emulated_clock_frequency}, is_native{is_native} {}

    u64 emulated_cpu_frequency;
    u64 emulated_clock_frequency;

private:
    bool is_native;
};

WallClock* CreateBestMatchingClock(u32 emulated_cpu_frequency, u32 emulated_clock_frequency);

} // namespace Common
