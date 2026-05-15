// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/steady_clock.h"
#include "common/uint128.h"
#include "common/wall_clock.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_detect.h"
#include "common/x64/rdtsc.h"
#endif

namespace Common {

#if defined(ARCHITECTURE_x86_64)
WallClock::WallClock(bool invariant_, u64 rdtsc_frequency_) noexcept
    : invariant{invariant_}
    , rdtsc_frequency{rdtsc_frequency_}
    , ns_rdtsc_factor{GetFixedPoint64Factor(NsRatio::den, rdtsc_frequency_)}
    , us_rdtsc_factor{GetFixedPoint64Factor(UsRatio::den, rdtsc_frequency_)}
    , ms_rdtsc_factor{GetFixedPoint64Factor(MsRatio::den, rdtsc_frequency_)}
    , cntpct_rdtsc_factor{GetFixedPoint64Factor(CNTFRQ, rdtsc_frequency_)}
    , gputick_rdtsc_factor{GetFixedPoint64Factor(GPUTickFreq, rdtsc_frequency_)}
{}

std::chrono::nanoseconds WallClock::GetTimeNS() const {
    if (invariant)
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
    return std::chrono::nanoseconds{MultiplyHigh(GetUptime(), ns_rdtsc_factor)};
}

std::chrono::microseconds WallClock::GetTimeUS() const {
    if (invariant)
        return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
    return std::chrono::microseconds{MultiplyHigh(GetUptime(), us_rdtsc_factor)};
}

std::chrono::milliseconds WallClock::GetTimeMS() const {
    if (invariant)
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
    return std::chrono::milliseconds{MultiplyHigh(GetUptime(), ms_rdtsc_factor)};
}

s64 WallClock::GetCNTPCT() const {
    if (invariant)
        return GetUptime() * NsToCNTPCTRatio::num / NsToCNTPCTRatio::den;
    return MultiplyHigh(GetUptime(), cntpct_rdtsc_factor);
}

s64 WallClock::GetGPUTick() const {
    if (invariant)
        return GetUptime() * NsToGPUTickRatio::num / NsToGPUTickRatio::den;
    return MultiplyHigh(GetUptime(), gputick_rdtsc_factor);
}

s64 WallClock::GetUptime() const {
    if (invariant)
        return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    return s64(Common::X64::FencedRDTSC());
}

bool WallClock::IsNative() const {
    if (invariant)
        return false;
    return true;
}
#elif defined(HAS_NCE)
namespace {

[[nodiscard]] WallClock::FactorType GetFixedPointFactor(u64 num, u64 den) noexcept {
    return (WallClock::FactorType(num) << 64) / den;
}

[[nodiscard]] u64 MultiplyHigh(u64 m, WallClock::FactorType factor) noexcept {
    return static_cast<u64>((m * factor) >> 64);
}

[[nodiscard]] s64 GetHostCNTFRQ() noexcept {
    u64 cntfrq_el0 = 0;
#ifdef ANDROID
    std::string_view board{""};
    char buffer[PROP_VALUE_MAX];
    int len{__system_property_get("ro.product.board", buffer)};
    board = std::string_view(buffer, static_cast<size_t>(len));
    if (board == "s5e9925") { // Exynos 2200
        cntfrq_el0 = 25600000;
    } else if (board == "exynos2100") { // Exynos 2100
        cntfrq_el0 = 26000000;
    } else if (board == "exynos9810") { // Exynos 9810
        cntfrq_el0 = 26000000;
    } else if (board == "s5e8825") { // Exynos 1280
        cntfrq_el0 = 26000000;
    } else {
        asm volatile("mrs %[cntfrq_el0], cntfrq_el0" : [cntfrq_el0] "=r"(cntfrq_el0));
    }
    return cntfrq_el0;
#else
    asm volatile("mrs %[cntfrq_el0], cntfrq_el0" : [cntfrq_el0] "=r"(cntfrq_el0));
    return cntfrq_el0;
#endif
}

} // namespace

WallClock::WallClock(bool invariant_, u64 rdtsc_frequency_) noexcept {
    const u64 host_cntfrq = std::max<u64>(GetHostCNTFRQ(), 1);
    ns_cntfrq_factor = GetFixedPointFactor(NsRatio::den, host_cntfrq);
    us_cntfrq_factor = GetFixedPointFactor(UsRatio::den, host_cntfrq);
    ms_cntfrq_factor = GetFixedPointFactor(MsRatio::den, host_cntfrq);
    guest_cntfrq_factor = GetFixedPointFactor(CNTFRQ, host_cntfrq);
    gputick_cntfrq_factor = GetFixedPointFactor(GPUTickFreq, host_cntfrq);
}

std::chrono::nanoseconds WallClock::GetTimeNS() const {
    return std::chrono::nanoseconds{MultiplyHigh(GetUptime(), ns_cntfrq_factor)};
}

std::chrono::microseconds WallClock::GetTimeUS() const {
    return std::chrono::microseconds{MultiplyHigh(GetUptime(), us_cntfrq_factor)};
}

std::chrono::milliseconds WallClock::GetTimeMS() const {
    return std::chrono::milliseconds{MultiplyHigh(GetUptime(), ms_cntfrq_factor)};
}

s64 WallClock::GetCNTPCT() const {
    return MultiplyHigh(GetUptime(), guest_cntfrq_factor);
}

s64 WallClock::GetGPUTick() const {
    return MultiplyHigh(GetUptime(), gputick_cntfrq_factor);
}

s64 WallClock::GetUptime() const {
    s64 cntvct_el0 = 0;
    asm volatile(
        "dsb ish\n\t"
        "mrs %[cntvct_el0], cntvct_el0\n\t"
        "dsb ish\n\t"
        : [cntvct_el0] "=r"(cntvct_el0)
    );
    return cntvct_el0;
}

bool WallClock::IsNative() const {
    return true;
}
#else
WallClock::WallClock(bool invariant_, u64 rdtsc_frequency_) noexcept {}

std::chrono::nanoseconds WallClock::GetTimeNS() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch());
}

std::chrono::microseconds WallClock::GetTimeUS() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch());
}

std::chrono::milliseconds WallClock::GetTimeMS() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch());
}

s64 WallClock::GetCNTPCT() const {
    return GetUptime() * NsToCNTPCTRatio::num / NsToCNTPCTRatio::den;
}

s64 WallClock::GetGPUTick() const {
    return GetUptime() * NsToGPUTickRatio::num / NsToGPUTickRatio::den;
}

s64 WallClock::GetUptime() const {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

bool WallClock::IsNative() const {
    return false;
}
#endif

WallClock CreateOptimalClock() noexcept {
#if defined(ARCHITECTURE_x86_64)
    auto const& caps = GetCPUCaps();
    return WallClock(!(caps.invariant_tsc && caps.tsc_frequency >= std::nano::den), std::max<u64>(caps.tsc_frequency, 1));
#elif defined(HAS_NCE)
    return WallClock(false, 1);
#else
    return WallClock(true, 1);
#endif
}

} // namespace Common
