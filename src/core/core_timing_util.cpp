// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "core/core_timing_util.h"

#include <cinttypes>
#include <limits>
#include "common/logging/log.h"
#include "common/uint128.h"

namespace Core::Timing {

constexpr u64 MAX_VALUE_TO_MULTIPLY = std::numeric_limits<s64>::max() / Hardware::BASE_CLOCK_RATE;

s64 msToCycles(std::chrono::milliseconds ms) {
    if (static_cast<u64>(ms.count() / 1000) > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (static_cast<u64>(ms.count()) > MAX_VALUE_TO_MULTIPLY) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return Hardware::BASE_CLOCK_RATE * (ms.count() / 1000);
    }
    return (Hardware::BASE_CLOCK_RATE * ms.count()) / 1000;
}

s64 usToCycles(std::chrono::microseconds us) {
    if (static_cast<u64>(us.count() / 1000000) > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (static_cast<u64>(us.count()) > MAX_VALUE_TO_MULTIPLY) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return Hardware::BASE_CLOCK_RATE * (us.count() / 1000000);
    }
    return (Hardware::BASE_CLOCK_RATE * us.count()) / 1000000;
}

s64 nsToCycles(std::chrono::nanoseconds ns) {
    if (static_cast<u64>(ns.count() / 1000000000) > MAX_VALUE_TO_MULTIPLY) {
        LOG_ERROR(Core_Timing, "Integer overflow, use max value");
        return std::numeric_limits<s64>::max();
    }
    if (static_cast<u64>(ns.count()) > MAX_VALUE_TO_MULTIPLY) {
        LOG_DEBUG(Core_Timing, "Time very big, do rounding");
        return Hardware::BASE_CLOCK_RATE * (ns.count() / 1000000000);
    }
    return (Hardware::BASE_CLOCK_RATE * ns.count()) / 1000000000;
}

u64 CpuCyclesToClockCycles(u64 ticks) {
    const u128 temporal = Common::Multiply64Into128(ticks, Hardware::CNTFREQ);
    return Common::Divide128On32(temporal, static_cast<u32>(Hardware::BASE_CLOCK_RATE)).first;
}

} // namespace Core::Timing
