// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <chrono>

#include "common/common_types.h"
#include "common/intrusive_list.h"
#include "common/uuid.h"
#include "common/wall_clock.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/psc/time/errors.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
using ClockSourceId = Common::UUID;

struct SteadyClockTimePoint {
    constexpr bool IdMatches(SteadyClockTimePoint& other) {
        return clock_source_id == other.clock_source_id;
    }
    bool operator==(const SteadyClockTimePoint& other) const = default;

    s64 time_point;
    ClockSourceId clock_source_id;
};
static_assert(sizeof(SteadyClockTimePoint) == 0x18, "SteadyClockTimePoint has the wrong size!");
static_assert(std::is_trivial_v<ClockSourceId>);

struct SystemClockContext {
    bool operator==(const SystemClockContext& other) const = default;

    s64 offset;
    SteadyClockTimePoint steady_time_point;
};
static_assert(sizeof(SystemClockContext) == 0x20, "SystemClockContext has the wrong size!");
static_assert(std::is_trivial_v<SystemClockContext>);

enum class TimeType : u8 {
    UserSystemClock,
    NetworkSystemClock,
    LocalSystemClock,
};

struct CalendarTime {
    s16 year;
    s8 month;
    s8 day;
    s8 hour;
    s8 minute;
    s8 second;
};
static_assert(sizeof(CalendarTime) == 0x8, "CalendarTime has the wrong size!");

struct CalendarAdditionalInfo {
    s32 day_of_week;
    s32 day_of_year;
    std::array<char, 8> name;
    s32 is_dst;
    s32 ut_offset;
};
static_assert(sizeof(CalendarAdditionalInfo) == 0x18, "CalendarAdditionalInfo has the wrong size!");

struct LocationName {
    std::array<char, 36> name;
};
static_assert(sizeof(LocationName) == 0x24, "LocationName has the wrong size!");

struct RuleVersion {
    std::array<char, 16> version;
};
static_assert(sizeof(RuleVersion) == 0x10, "RuleVersion has the wrong size!");

struct ClockSnapshot {
    SystemClockContext user_context;
    SystemClockContext network_context;
    s64 user_time;
    s64 network_time;
    CalendarTime user_calendar_time;
    CalendarTime network_calendar_time;
    CalendarAdditionalInfo user_calendar_additional_time;
    CalendarAdditionalInfo network_calendar_additional_time;
    SteadyClockTimePoint steady_clock_time_point;
    LocationName location_name;
    bool is_automatic_correction_enabled;
    TimeType type;
    u16 unk_CE;
};
static_assert(sizeof(ClockSnapshot) == 0xD0, "ClockSnapshot has the wrong size!");
static_assert(std::is_trivial_v<ClockSnapshot>);

struct ContinuousAdjustmentTimePoint {
    s64 rtc_offset;
    s64 diff_scale;
    s64 shift_amount;
    s64 lower;
    s64 upper;
    ClockSourceId clock_source_id;
};
static_assert(sizeof(ContinuousAdjustmentTimePoint) == 0x38,
              "ContinuousAdjustmentTimePoint has the wrong size!");
static_assert(std::is_trivial_v<ContinuousAdjustmentTimePoint>);

struct AlarmInfo {
    s64 alert_time;
    u32 priority;
};
static_assert(sizeof(AlarmInfo) == 0x10, "AlarmInfo has the wrong size!");

struct StaticServiceSetupInfo {
    bool can_write_local_clock;
    bool can_write_user_clock;
    bool can_write_network_clock;
    bool can_write_timezone_device_location;
    bool can_write_steady_clock;
    bool can_write_uninitialized_clock;
};
static_assert(sizeof(StaticServiceSetupInfo) == 0x6, "StaticServiceSetupInfo has the wrong size!");

struct OperationEvent : public Common::IntrusiveListBaseNode<OperationEvent> {
    using OperationEventList = Common::IntrusiveListBaseTraits<OperationEvent>::ListType;

    OperationEvent(Core::System& system);
    ~OperationEvent();

    KernelHelpers::ServiceContext m_ctx;
    Kernel::KEvent* m_event{};
};

constexpr inline std::chrono::nanoseconds ConvertToTimeSpan(s64 ticks) {
    constexpr auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};

    constexpr s64 max{Common::WallClock::CNTFRQ *
                      (std::numeric_limits<s64>::max() / one_second_ns)};

    if (ticks > max) {
        return std::chrono::nanoseconds(std::numeric_limits<s64>::max());
    } else if (ticks < -max) {
        return std::chrono::nanoseconds(std::numeric_limits<s64>::min());
    }

    auto a{ticks / Common::WallClock::CNTFRQ * one_second_ns};
    auto b{((ticks % Common::WallClock::CNTFRQ) * one_second_ns) / Common::WallClock::CNTFRQ};

    return std::chrono::nanoseconds(a + b);
}

constexpr inline Result GetSpanBetweenTimePoints(s64* out_seconds, SteadyClockTimePoint& a,
                                                 SteadyClockTimePoint& b) {
    R_UNLESS(out_seconds, ResultInvalidArgument);
    R_UNLESS(a.IdMatches(b), ResultInvalidArgument);
    R_UNLESS(a.time_point >= 0 || b.time_point <= a.time_point + std::numeric_limits<s64>::max(),
             ResultOverflow);
    R_UNLESS(a.time_point < 0 || b.time_point >= a.time_point + std::numeric_limits<s64>::min(),
             ResultOverflow);

    *out_seconds = b.time_point - a.time_point;
    R_SUCCEED();
}

} // namespace Service::PSC::Time
