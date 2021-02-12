// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

// This file references various implementation details from Atmosphere, an open-source firmware for
// the Nintendo Switch. Copyright 2018-2020 Atmosphere-NX.

#include "common/assert.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/kernel/k_resource_limit.h"
#include "core/hle/kernel/svc_results.h"

namespace Kernel {
constexpr s64 DefaultTimeout = 10000000000; // 10 seconds

KResourceLimit::KResourceLimit(KernelCore& kernel, Core::System& system)
    : Object{kernel}, lock{kernel}, cond_var{kernel}, kernel{kernel}, system(system) {}
KResourceLimit::~KResourceLimit() = default;

s64 KResourceLimit::GetLimitValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk{lock};
        value = limit_values[index];
        ASSERT(value >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
    }
    return value;
}

s64 KResourceLimit::GetCurrentValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk{lock};
        value = current_values[index];
        ASSERT(value >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
    }
    return value;
}

s64 KResourceLimit::GetPeakValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk{lock};
        value = peak_values[index];
        ASSERT(value >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
    }
    return value;
}

s64 KResourceLimit::GetFreeValue(LimitableResource which) const {
    const auto index = static_cast<std::size_t>(which);
    s64 value{};
    {
        KScopedLightLock lk(lock);
        ASSERT(current_values[index] >= 0);
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);
        value = limit_values[index] - current_values[index];
    }

    return value;
}

ResultCode KResourceLimit::SetLimitValue(LimitableResource which, s64 value) {
    const auto index = static_cast<std::size_t>(which);
    KScopedLightLock lk(lock);
    R_UNLESS(current_values[index] <= value, ResultInvalidState);

    limit_values[index] = value;

    return RESULT_SUCCESS;
}

bool KResourceLimit::Reserve(LimitableResource which, s64 value) {
    return Reserve(which, value, system.CoreTiming().GetGlobalTimeNs().count() + DefaultTimeout);
}

bool KResourceLimit::Reserve(LimitableResource which, s64 value, s64 timeout) {
    ASSERT(value >= 0);
    const auto index = static_cast<std::size_t>(which);
    KScopedLightLock lk(lock);

    ASSERT(current_hints[index] <= current_values[index]);
    if (current_hints[index] >= limit_values[index]) {
        return false;
    }

    // Loop until we reserve or run out of time.
    while (true) {
        ASSERT(current_values[index] <= limit_values[index]);
        ASSERT(current_hints[index] <= current_values[index]);

        // If we would overflow, don't allow to succeed.
        if (current_values[index] + value <= current_values[index]) {
            break;
        }

        if (current_values[index] + value <= limit_values[index]) {
            current_values[index] += value;
            current_hints[index] += value;
            peak_values[index] = std::max(peak_values[index], current_values[index]);
            return true;
        }

        if (current_hints[index] + value <= limit_values[index] &&
            (timeout < 0 || system.CoreTiming().GetGlobalTimeNs().count() < timeout)) {
            waiter_count++;
            cond_var.Wait(&lock, timeout);
            waiter_count--;
        } else {
            break;
        }
    }

    return false;
}

void KResourceLimit::Release(LimitableResource which, s64 value) {
    Release(which, value, value);
}

void KResourceLimit::Release(LimitableResource which, s64 value, s64 hint) {
    ASSERT(value >= 0);
    ASSERT(hint >= 0);

    const auto index = static_cast<std::size_t>(which);
    KScopedLightLock lk(lock);
    ASSERT(current_values[index] <= limit_values[index]);
    ASSERT(current_hints[index] <= current_values[index]);
    ASSERT(value <= current_values[index]);
    ASSERT(hint <= current_hints[index]);

    current_values[index] -= value;
    current_hints[index] -= hint;

    if (waiter_count != 0) {
        cond_var.Broadcast();
    }
}

} // namespace Kernel
