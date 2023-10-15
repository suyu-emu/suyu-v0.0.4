// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/arm64/native_clock.h"

namespace Common::Arm64 {

namespace {

NativeClock::FactorType GetFixedPointFactor(u64 num, u64 den) {
    return (static_cast<NativeClock::FactorType>(num) << 64) / den;
}

u64 MultiplyHigh(u64 m, NativeClock::FactorType factor) {
    return static_cast<u64>((m * factor) >> 64);
}

} // namespace

NativeClock::NativeClock() {
    const u64 host_cntfrq = GetHostCNTFRQ();
    ns_cntfrq_factor = GetFixedPointFactor(NsRatio::den, host_cntfrq);
    us_cntfrq_factor = GetFixedPointFactor(UsRatio::den, host_cntfrq);
    ms_cntfrq_factor = GetFixedPointFactor(MsRatio::den, host_cntfrq);
    guest_cntfrq_factor = GetFixedPointFactor(CNTFRQ, host_cntfrq);
    gputick_cntfrq_factor = GetFixedPointFactor(GPUTickFreq, host_cntfrq);
}

std::chrono::nanoseconds NativeClock::GetTimeNS() const {
    return std::chrono::nanoseconds{MultiplyHigh(GetHostTicksElapsed(), ns_cntfrq_factor)};
}

std::chrono::microseconds NativeClock::GetTimeUS() const {
    return std::chrono::microseconds{MultiplyHigh(GetHostTicksElapsed(), us_cntfrq_factor)};
}

std::chrono::milliseconds NativeClock::GetTimeMS() const {
    return std::chrono::milliseconds{MultiplyHigh(GetHostTicksElapsed(), ms_cntfrq_factor)};
}

u64 NativeClock::GetCNTPCT() const {
    return MultiplyHigh(GetHostTicksElapsed(), guest_cntfrq_factor);
}

u64 NativeClock::GetGPUTick() const {
    return MultiplyHigh(GetHostTicksElapsed(), gputick_cntfrq_factor);
}

u64 NativeClock::GetHostTicksNow() const {
    u64 cntvct_el0 = 0;
    asm volatile("dsb ish\n\t"
                 "mrs %[cntvct_el0], cntvct_el0\n\t"
                 "dsb ish\n\t"
                 : [cntvct_el0] "=r"(cntvct_el0));
    return cntvct_el0;
}

u64 NativeClock::GetHostTicksElapsed() const {
    return GetHostTicksNow();
}

bool NativeClock::IsNative() const {
    return true;
}

u64 NativeClock::GetHostCNTFRQ() {
    u64 cntfrq_el0 = 0;
    asm("mrs %[cntfrq_el0], cntfrq_el0" : [cntfrq_el0] "=r"(cntfrq_el0));
    return cntfrq_el0;
}

} // namespace Common::Arm64
