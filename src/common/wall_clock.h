// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <memory>
#include <ratio>

#include "common/common_types.h"

namespace Common {

class WallClock {
public:
    static constexpr u64 CNTFRQ = 19'200'000;         // CNTPCT_EL0 Frequency = 19.2 MHz
    static constexpr u64 GPUTickFreq = 614'400'000;   // GM20B GPU Tick Frequency = 614.4 MHz
    static constexpr u64 CPUTickFreq = 1'020'000'000; // T210/4 A57 CPU Tick Frequency = 1020.0 MHz

    explicit WallClock(bool invariant, u64 rdtsc_frequency_) noexcept;

    /// @returns The time in nanoseconds since the construction of this clock.
    std::chrono::nanoseconds GetTimeNS() const;

    /// @returns The time in microseconds since the construction of this clock.
    std::chrono::microseconds GetTimeUS() const;

    /// @returns The time in milliseconds since the construction of this clock.
    std::chrono::milliseconds GetTimeMS() const;

    /// @returns The guest CNTPCT ticks since the construction of this clock.
    s64 GetCNTPCT() const;

    /// @returns The guest GPU ticks since the construction of this clock.
    s64 GetGPUTick() const;

    /// @returns The raw host timer ticks since an indeterminate epoch.
    s64 GetUptime() const;

    /// @returns Whether the clock directly uses the host's hardware clock.
    bool IsNative() const;

    static inline u64 NSToCNTPCT(u64 ns) {
        return ns * NsToCNTPCTRatio::num / NsToCNTPCTRatio::den;
    }

    static inline u64 NSToGPUTick(u64 ns) {
        return ns * NsToGPUTickRatio::num / NsToGPUTickRatio::den;
    }

    // Cycle Timing

    static inline u64 CPUTickToNS(u64 cpu_tick) {
        return cpu_tick * CPUTickToNsRatio::num / CPUTickToNsRatio::den;
    }

    static inline u64 CPUTickToUS(u64 cpu_tick) {
        return cpu_tick * CPUTickToUsRatio::num / CPUTickToUsRatio::den;
    }

    static inline u64 CPUTickToCNTPCT(u64 cpu_tick) {
        return cpu_tick * CPUTickToCNTPCTRatio::num / CPUTickToCNTPCTRatio::den;
    }

    static inline u64 CPUTickToGPUTick(u64 cpu_tick) {
        return cpu_tick * CPUTickToGPUTickRatio::num / CPUTickToGPUTickRatio::den;
    }

protected:
    using NsRatio = std::nano;
    using UsRatio = std::micro;
    using MsRatio = std::milli;

    using NsToUsRatio = std::ratio_divide<std::nano, std::micro>;
    using NsToMsRatio = std::ratio_divide<std::nano, std::milli>;
    using NsToCNTPCTRatio = std::ratio<CNTFRQ, std::nano::den>;
    using NsToGPUTickRatio = std::ratio<GPUTickFreq, std::nano::den>;

    // Cycle Timing

    using CPUTickToNsRatio = std::ratio<std::nano::den, CPUTickFreq>;
    using CPUTickToUsRatio = std::ratio<std::micro::den, CPUTickFreq>;
    using CPUTickToCNTPCTRatio = std::ratio<CNTFRQ, CPUTickFreq>;
    using CPUTickToGPUTickRatio = std::ratio<GPUTickFreq, CPUTickFreq>;

#if defined(ARCHITECTURE_x86_64)
    bool invariant;
    u64 rdtsc_frequency;
    u64 ns_rdtsc_factor;
    u64 us_rdtsc_factor;
    u64 ms_rdtsc_factor;
    u64 cntpct_rdtsc_factor;
    u64 gputick_rdtsc_factor;
#elif defined(HAS_NCE)
public:
    using FactorType = unsigned __int128;

    FactorType GetGuestCNTFRQFactor() const {
        return guest_cntfrq_factor;
    }
protected:
    FactorType ns_cntfrq_factor;
    FactorType us_cntfrq_factor;
    FactorType ms_cntfrq_factor;
    FactorType guest_cntfrq_factor;
    FactorType gputick_cntfrq_factor;
#else

#endif
};

[[nodiscard]] WallClock CreateOptimalClock() noexcept;

} // namespace Common
