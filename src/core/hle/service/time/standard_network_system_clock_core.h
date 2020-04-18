// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/steady_clock_core.h"
#include "core/hle/service/time/system_clock_core.h"

namespace Core {
class System;
}

namespace Service::Time::Clock {

class StandardNetworkSystemClockCore final : public SystemClockCore {
public:
    explicit StandardNetworkSystemClockCore(SteadyClockCore& steady_clock_core)
        : SystemClockCore{steady_clock_core} {}

    void SetStandardNetworkClockSufficientAccuracy(TimeSpanType value) {
        standard_network_clock_sufficient_accuracy = value;
    }

    bool IsStandardNetworkSystemClockAccuracySufficient(Core::System& system) const {
        SystemClockContext context{};
        if (GetClockContext(system, context) != RESULT_SUCCESS) {
            return {};
        }

        s64 span{};
        if (context.steady_time_point.GetSpanBetween(
                GetSteadyClockCore().GetCurrentTimePoint(system), span) != RESULT_SUCCESS) {
            return {};
        }

        return TimeSpanType{span}.nanoseconds <
               standard_network_clock_sufficient_accuracy.nanoseconds;
    }

private:
    TimeSpanType standard_network_clock_sufficient_accuracy{};
};

} // namespace Service::Time::Clock
