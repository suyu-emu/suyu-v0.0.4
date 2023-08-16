// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/uint128.h"
#include "common/x64/native_clock.h"
#include "common/x64/rdtsc.h"

namespace Common::X64 {

NativeClock::NativeClock(u64 rdtsc_frequency_)
    : start_ticks{FencedRDTSC()}, rdtsc_frequency{rdtsc_frequency_},
      ns_rdtsc_factor{GetFixedPoint64Factor(NsRatio::den, rdtsc_frequency)},
      us_rdtsc_factor{GetFixedPoint64Factor(UsRatio::den, rdtsc_frequency)},
      ms_rdtsc_factor{GetFixedPoint64Factor(MsRatio::den, rdtsc_frequency)},
      cntpct_rdtsc_factor{GetFixedPoint64Factor(CNTFRQ, rdtsc_frequency)},
      gputick_rdtsc_factor{GetFixedPoint64Factor(GPUTickFreq, rdtsc_frequency)} {}

std::chrono::nanoseconds NativeClock::GetTimeNS() const {
    return std::chrono::nanoseconds{MultiplyHigh(GetHostTicksElapsed(), ns_rdtsc_factor)};
}

std::chrono::microseconds NativeClock::GetTimeUS() const {
    return std::chrono::microseconds{MultiplyHigh(GetHostTicksElapsed(), us_rdtsc_factor)};
}

std::chrono::milliseconds NativeClock::GetTimeMS() const {
    return std::chrono::milliseconds{MultiplyHigh(GetHostTicksElapsed(), ms_rdtsc_factor)};
}

u64 NativeClock::GetCNTPCT() const {
    return MultiplyHigh(GetHostTicksElapsed(), cntpct_rdtsc_factor);
}

u64 NativeClock::GetGPUTick() const {
    return MultiplyHigh(GetHostTicksElapsed(), gputick_rdtsc_factor);
}

u64 NativeClock::GetHostTicksNow() const {
    return FencedRDTSC();
}

u64 NativeClock::GetHostTicksElapsed() const {
    return FencedRDTSC() - start_ticks;
}

bool NativeClock::IsNative() const {
    return true;
}

} // namespace Common::X64
