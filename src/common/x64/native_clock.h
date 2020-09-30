// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>

#include "common/spin_lock.h"
#include "common/wall_clock.h"

namespace Common {

namespace X64 {
class NativeClock final : public WallClock {
public:
    NativeClock(u64 emulated_cpu_frequency, u64 emulated_clock_frequency, u64 rtsc_frequency);

    std::chrono::nanoseconds GetTimeNS() override;

    std::chrono::microseconds GetTimeUS() override;

    std::chrono::milliseconds GetTimeMS() override;

    u64 GetClockCycles() override;

    u64 GetCPUCycles() override;

    void Pause(bool is_paused) override;

private:
    u64 GetRTSC();

    /// value used to reduce the native clocks accuracy as some apss rely on
    /// undefined behavior where the level of accuracy in the clock shouldn't
    /// be higher.
    static constexpr u64 inaccuracy_mask = ~(0x400 - 1);

    SpinLock rtsc_serialize{};
    u64 last_measure{};
    u64 accumulated_ticks{};
    u64 rtsc_frequency;
};
} // namespace X64

u64 EstimateRDTSCFrequency();

} // namespace Common
