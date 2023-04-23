// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/wall_clock.h"

namespace Common::X64 {

class NativeClock final : public WallClock {
public:
    explicit NativeClock(u64 rdtsc_frequency_);

    std::chrono::nanoseconds GetTimeNS() const override;

    std::chrono::microseconds GetTimeUS() const override;

    std::chrono::milliseconds GetTimeMS() const override;

    u64 GetCNTPCT() const override;

    u64 GetHostTicksNow() const override;

    u64 GetHostTicksElapsed() const override;

    bool IsNative() const override;

private:
    u64 start_ticks;
    u64 rdtsc_frequency;

    u64 ns_rdtsc_factor;
    u64 us_rdtsc_factor;
    u64 ms_rdtsc_factor;
    u64 cntpct_rdtsc_factor;
};

} // namespace Common::X64
