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
    static constexpr u64 CNTFRQ = 19'200'000; // CNTPCT_EL0 Frequency = 19.2 MHz

    virtual ~WallClock() = default;

    /// @returns The time in nanoseconds since the construction of this clock.
    virtual std::chrono::nanoseconds GetTimeNS() const = 0;

    /// @returns The time in microseconds since the construction of this clock.
    virtual std::chrono::microseconds GetTimeUS() const = 0;

    /// @returns The time in milliseconds since the construction of this clock.
    virtual std::chrono::milliseconds GetTimeMS() const = 0;

    /// @returns The guest CNTPCT ticks since the construction of this clock.
    virtual u64 GetCNTPCT() const = 0;

    /// @returns The raw host timer ticks since an indeterminate epoch.
    virtual u64 GetHostTicksNow() const = 0;

    /// @returns The raw host timer ticks since the construction of this clock.
    virtual u64 GetHostTicksElapsed() const = 0;

    /// @returns Whether the clock directly uses the host's hardware clock.
    virtual bool IsNative() const = 0;

    static inline u64 NSToCNTPCT(u64 ns) {
        return ns * NsToCNTPCTRatio::num / NsToCNTPCTRatio::den;
    }

    static inline u64 USToCNTPCT(u64 us) {
        return us * UsToCNTPCTRatio::num / UsToCNTPCTRatio::den;
    }

    static inline u64 CNTPCTToNS(u64 cntpct) {
        return cntpct * NsToCNTPCTRatio::den / NsToCNTPCTRatio::num;
    }

    static inline u64 CNTPCTToUS(u64 cntpct) {
        return cntpct * UsToCNTPCTRatio::den / UsToCNTPCTRatio::num;
    }

protected:
    using NsRatio = std::nano;
    using UsRatio = std::micro;
    using MsRatio = std::milli;

    using NsToUsRatio = std::ratio_divide<std::nano, std::micro>;
    using NsToMsRatio = std::ratio_divide<std::nano, std::milli>;
    using NsToCNTPCTRatio = std::ratio<CNTFRQ, std::nano::den>;
    using UsToCNTPCTRatio = std::ratio<CNTFRQ, std::micro::den>;
};

std::unique_ptr<WallClock> CreateOptimalClock();

std::unique_ptr<WallClock> CreateStandardWallClock();

} // namespace Common
