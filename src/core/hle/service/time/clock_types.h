// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <ratio>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/uuid.h"
#include "core/hle/service/time/errors.h"
#include "core/hle/service/time/time_zone_types.h"

// Defined by WinBase.h on Windows
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif

namespace Service::Time::Clock {

enum class TimeType : u8 {
    UserSystemClock,
    NetworkSystemClock,
    LocalSystemClock,
};

/// https://switchbrew.org/wiki/Glue_services#SteadyClockTimePoint
struct SteadyClockTimePoint {
    s64 time_point;
    Common::UUID clock_source_id;

    Result GetSpanBetween(SteadyClockTimePoint other, s64& span) const {
        span = 0;

        if (clock_source_id != other.clock_source_id) {
            return ERROR_TIME_MISMATCH;
        }

        span = other.time_point - time_point;

        return ResultSuccess;
    }

    static SteadyClockTimePoint GetRandom() {
        return {0, Common::UUID::MakeRandom()};
    }
};
static_assert(sizeof(SteadyClockTimePoint) == 0x18, "SteadyClockTimePoint is incorrect size");
static_assert(std::is_trivially_copyable_v<SteadyClockTimePoint>,
              "SteadyClockTimePoint must be trivially copyable");

struct SteadyClockContext {
    u64 internal_offset;
    Common::UUID steady_time_point;
};
static_assert(sizeof(SteadyClockContext) == 0x18, "SteadyClockContext is incorrect size");
static_assert(std::is_trivially_copyable_v<SteadyClockContext>,
              "SteadyClockContext must be trivially copyable");
using StandardSteadyClockTimePointType = SteadyClockContext;

struct SystemClockContext {
    s64 offset;
    SteadyClockTimePoint steady_time_point;
};
static_assert(sizeof(SystemClockContext) == 0x20, "SystemClockContext is incorrect size");
static_assert(std::is_trivially_copyable_v<SystemClockContext>,
              "SystemClockContext must be trivially copyable");

struct ContinuousAdjustmentTimePoint {
    s64 measurement_offset;
    s64 diff_scale;
    u32 shift_amount;
    s64 lower;
    s64 upper;
    Common::UUID clock_source_id;
};
static_assert(sizeof(ContinuousAdjustmentTimePoint) == 0x38);
static_assert(std::is_trivially_copyable_v<ContinuousAdjustmentTimePoint>,
              "ContinuousAdjustmentTimePoint must be trivially copyable");

/// https://switchbrew.org/wiki/Glue_services#TimeSpanType
struct TimeSpanType {
    s64 nanoseconds{};

    s64 ToSeconds() const {
        return nanoseconds / std::nano::den;
    }

    static TimeSpanType FromSeconds(s64 seconds) {
        return {seconds * std::nano::den};
    }

    template <u64 Frequency>
    static TimeSpanType FromTicks(u64 ticks) {
        using TicksToNSRatio = std::ratio<std::nano::den, Frequency>;
        return {static_cast<s64>(ticks * TicksToNSRatio::num / TicksToNSRatio::den)};
    }
};
static_assert(sizeof(TimeSpanType) == 8, "TimeSpanType is incorrect size");

struct ClockSnapshot {
    SystemClockContext user_context;
    SystemClockContext network_context;
    s64 user_time;
    s64 network_time;
    TimeZone::CalendarTime user_calendar_time;
    TimeZone::CalendarTime network_calendar_time;
    TimeZone::CalendarAdditionalInfo user_calendar_additional_time;
    TimeZone::CalendarAdditionalInfo network_calendar_additional_time;
    SteadyClockTimePoint steady_clock_time_point;
    TimeZone::LocationName location_name;
    u8 is_automatic_correction_enabled;
    TimeType type;
    INSERT_PADDING_BYTES_NOINIT(0x2);

    static Result GetCurrentTime(s64& current_time,
                                 const SteadyClockTimePoint& steady_clock_time_point,
                                 const SystemClockContext& context) {
        if (steady_clock_time_point.clock_source_id != context.steady_time_point.clock_source_id) {
            current_time = 0;
            return ERROR_TIME_MISMATCH;
        }
        current_time = steady_clock_time_point.time_point + context.offset;
        return ResultSuccess;
    }
};
static_assert(sizeof(ClockSnapshot) == 0xD0, "ClockSnapshot is incorrect size");

} // namespace Service::Time::Clock
