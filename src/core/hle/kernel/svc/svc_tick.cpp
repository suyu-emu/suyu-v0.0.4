// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

/// This returns the total CPU ticks elapsed since the CPU was powered-on
u64 GetSystemTick(Core::System& system) {
    LOG_TRACE(Kernel_SVC, "called");

    auto& core_timing = system.CoreTiming();

    // Returns the value of cntpct_el0 (https://switchbrew.org/wiki/SVC#svcGetSystemTick)
    const u64 result{core_timing.GetClockTicks()};

    if (!system.Kernel().IsMulticore()) {
        core_timing.AddTicks(400U);
    }

    return result;
}

void GetSystemTick32(Core::System& system, u32* time_low, u32* time_high) {
    const auto time = GetSystemTick(system);
    *time_low = static_cast<u32>(time);
    *time_high = static_cast<u32>(time >> 32);
}

} // namespace Kernel::Svc
