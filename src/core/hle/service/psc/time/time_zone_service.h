// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Tz {
struct Rule;
}

namespace Service::PSC::Time {

class TimeZoneService final : public ServiceFramework<TimeZoneService> {
public:
    explicit TimeZoneService(Core::System& system, StandardSteadyClockCore& clock_core,
                             TimeZone& time_zone, bool can_write_timezone_device_location);

    ~TimeZoneService() override = default;

    Result GetDeviceLocationName(LocationName& out_location_name);
    Result GetTotalLocationNameCount(u32& out_count);
    Result GetTimeZoneRuleVersion(RuleVersion& out_rule_version);
    Result GetDeviceLocationNameAndUpdatedTime(SteadyClockTimePoint& out_time_point,
                                               LocationName& location_name);
    Result SetDeviceLocationNameWithTimeZoneRule(LocationName& location_name,
                                                 std::span<const u8> binary);
    Result ParseTimeZoneBinary(Tz::Rule& out_rule, std::span<const u8> binary);
    Result ToCalendarTime(CalendarTime& out_calendar_time,
                          CalendarAdditionalInfo& out_additional_info, s64 time, Tz::Rule& rule);
    Result ToCalendarTimeWithMyRule(CalendarTime& out_calendar_time,
                                    CalendarAdditionalInfo& out_additional_info, s64 time);
    Result ToPosixTime(u32& out_count, std::span<s64, 2> out_times, u32 out_times_count,
                       CalendarTime& calendar_time, Tz::Rule& rule);
    Result ToPosixTimeWithMyRule(u32& out_count, std::span<s64, 2> out_times, u32 out_times_count,
                                 CalendarTime& calendar_time);

private:
    void Handle_GetDeviceLocationName(HLERequestContext& ctx);
    void Handle_SetDeviceLocationName(HLERequestContext& ctx);
    void Handle_GetTotalLocationNameCount(HLERequestContext& ctx);
    void Handle_LoadLocationNameList(HLERequestContext& ctx);
    void Handle_LoadTimeZoneRule(HLERequestContext& ctx);
    void Handle_GetTimeZoneRuleVersion(HLERequestContext& ctx);
    void Handle_GetDeviceLocationNameAndUpdatedTime(HLERequestContext& ctx);
    void Handle_SetDeviceLocationNameWithTimeZoneRule(HLERequestContext& ctx);
    void Handle_ParseTimeZoneBinary(HLERequestContext& ctx);
    void Handle_GetDeviceLocationNameOperationEventReadableHandle(HLERequestContext& ctx);
    void Handle_ToCalendarTime(HLERequestContext& ctx);
    void Handle_ToCalendarTimeWithMyRule(HLERequestContext& ctx);
    void Handle_ToPosixTime(HLERequestContext& ctx);
    void Handle_ToPosixTimeWithMyRule(HLERequestContext& ctx);

    Core::System& m_system;

    StandardSteadyClockCore& m_clock_core;
    TimeZone& m_time_zone;
    bool m_can_write_timezone_device_location;
};

} // namespace Service::PSC::Time
