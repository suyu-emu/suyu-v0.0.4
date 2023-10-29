// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <tz/tz.h>
#include "core/core.h"
#include "core/hle/service/psc/time/time_zone_service.h"

namespace Service::PSC::Time {

TimeZoneService::TimeZoneService(Core::System& system_, StandardSteadyClockCore& clock_core,
                                 TimeZone& time_zone, bool can_write_timezone_device_location)
    : ServiceFramework{system_, "ITimeZoneService"}, m_system{system}, m_clock_core{clock_core},
      m_time_zone{time_zone}, m_can_write_timezone_device_location{
                                  can_write_timezone_device_location} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0,   &TimeZoneService::Handle_GetDeviceLocationName, "GetDeviceLocationName"},
        {1,   &TimeZoneService::Handle_SetDeviceLocationName, "SetDeviceLocationName"},
        {2,   &TimeZoneService::Handle_GetTotalLocationNameCount, "GetTotalLocationNameCount"},
        {3,   &TimeZoneService::Handle_LoadLocationNameList, "LoadLocationNameList"},
        {4,   &TimeZoneService::Handle_LoadTimeZoneRule, "LoadTimeZoneRule"},
        {5,   &TimeZoneService::Handle_GetTimeZoneRuleVersion, "GetTimeZoneRuleVersion"},
        {6,   &TimeZoneService::Handle_GetDeviceLocationNameAndUpdatedTime, "GetDeviceLocationNameAndUpdatedTime"},
        {7,   &TimeZoneService::Handle_SetDeviceLocationNameWithTimeZoneRule, "SetDeviceLocationNameWithTimeZoneRule"},
        {8,   &TimeZoneService::Handle_ParseTimeZoneBinary, "ParseTimeZoneBinary"},
        {20,  &TimeZoneService::Handle_GetDeviceLocationNameOperationEventReadableHandle, "GetDeviceLocationNameOperationEventReadableHandle"},
        {100, &TimeZoneService::Handle_ToCalendarTime, "ToCalendarTime"},
        {101, &TimeZoneService::Handle_ToCalendarTimeWithMyRule, "ToCalendarTimeWithMyRule"},
        {201, &TimeZoneService::Handle_ToPosixTime, "ToPosixTime"},
        {202, &TimeZoneService::Handle_ToPosixTimeWithMyRule, "ToPosixTimeWithMyRule"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

void TimeZoneService::Handle_GetDeviceLocationName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    LocationName name{};
    auto res = GetDeviceLocationName(name);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(LocationName) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<LocationName>(name);
}

void TimeZoneService::Handle_SetDeviceLocationName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    [[maybe_unused]] auto name{rp.PopRaw<LocationName>()};

    if (!m_can_write_timezone_device_location) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultPermissionDenied);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNotImplemented);
}

void TimeZoneService::Handle_GetTotalLocationNameCount(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    u32 count{};
    auto res = GetTotalLocationNameCount(count);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(count);
}

void TimeZoneService::Handle_LoadLocationNameList(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNotImplemented);
}

void TimeZoneService::Handle_LoadTimeZoneRule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNotImplemented);
}

void TimeZoneService::Handle_GetTimeZoneRuleVersion(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    RuleVersion rule_version{};
    auto res = GetTimeZoneRuleVersion(rule_version);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(RuleVersion) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<RuleVersion>(rule_version);
}

void TimeZoneService::Handle_GetDeviceLocationNameAndUpdatedTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    LocationName name{};
    SteadyClockTimePoint time_point{};
    auto res = GetDeviceLocationNameAndUpdatedTime(time_point, name);

    IPC::ResponseBuilder rb{ctx, 2 + (sizeof(LocationName) / sizeof(u32)) +
                                     (sizeof(SteadyClockTimePoint) / sizeof(u32))};
    rb.Push(res);
    rb.PushRaw<LocationName>(name);
    rb.PushRaw<SteadyClockTimePoint>(time_point);
}

void TimeZoneService::Handle_SetDeviceLocationNameWithTimeZoneRule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto name{rp.PopRaw<LocationName>()};

    auto binary{ctx.ReadBuffer()};
    auto res = SetDeviceLocationNameWithTimeZoneRule(name, binary);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void TimeZoneService::Handle_ParseTimeZoneBinary(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    auto binary{ctx.ReadBuffer()};

    Tz::Rule rule{};
    auto res = ParseTimeZoneBinary(rule, binary);

    ctx.WriteBuffer(rule);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void TimeZoneService::Handle_GetDeviceLocationNameOperationEventReadableHandle(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNotImplemented);
}

void TimeZoneService::Handle_ToCalendarTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto time{rp.Pop<s64>()};

    auto rule_buffer{ctx.ReadBuffer()};
    Tz::Rule rule{};
    std::memcpy(&rule, rule_buffer.data(), sizeof(Tz::Rule));

    CalendarTime calendar_time{};
    CalendarAdditionalInfo additional_info{};
    auto res = ToCalendarTime(calendar_time, additional_info, time, rule);

    IPC::ResponseBuilder rb{ctx, 2 + (sizeof(CalendarTime) / sizeof(u32)) +
                                     (sizeof(CalendarAdditionalInfo) / sizeof(u32))};
    rb.Push(res);
    rb.PushRaw<CalendarTime>(calendar_time);
    rb.PushRaw<CalendarAdditionalInfo>(additional_info);
}

void TimeZoneService::Handle_ToCalendarTimeWithMyRule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto time{rp.Pop<s64>()};

    CalendarTime calendar_time{};
    CalendarAdditionalInfo additional_info{};
    auto res = ToCalendarTimeWithMyRule(calendar_time, additional_info, time);

    IPC::ResponseBuilder rb{ctx, 2 + (sizeof(CalendarTime) / sizeof(u32)) +
                                     (sizeof(CalendarAdditionalInfo) / sizeof(u32))};
    rb.Push(res);
    rb.PushRaw<CalendarTime>(calendar_time);
    rb.PushRaw<CalendarAdditionalInfo>(additional_info);
}

void TimeZoneService::Handle_ToPosixTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto calendar{rp.PopRaw<CalendarTime>()};

    auto binary{ctx.ReadBuffer()};

    Tz::Rule rule{};
    std::memcpy(&rule, binary.data(), sizeof(Tz::Rule));

    u32 count{};
    std::array<s64, 2> times{};
    u32 times_count{static_cast<u32>(ctx.GetWriteBufferSize() / sizeof(s64))};

    auto res = ToPosixTime(count, times, times_count, calendar, rule);

    ctx.WriteBuffer(times);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(count);
}

void TimeZoneService::Handle_ToPosixTimeWithMyRule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto calendar{rp.PopRaw<CalendarTime>()};

    u32 count{};
    std::array<s64, 2> times{};
    u32 times_count{static_cast<u32>(ctx.GetWriteBufferSize() / sizeof(s64))};

    auto res = ToPosixTimeWithMyRule(count, times, times_count, calendar);

    ctx.WriteBuffer(times);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(count);
}

// =============================== Implementations ===========================

Result TimeZoneService::GetDeviceLocationName(LocationName& out_location_name) {
    R_RETURN(m_time_zone.GetLocationName(out_location_name));
}

Result TimeZoneService::GetTotalLocationNameCount(u32& out_count) {
    R_RETURN(m_time_zone.GetTotalLocationCount(out_count));
}

Result TimeZoneService::GetTimeZoneRuleVersion(RuleVersion& out_rule_version) {
    R_RETURN(m_time_zone.GetRuleVersion(out_rule_version));
}

Result TimeZoneService::GetDeviceLocationNameAndUpdatedTime(SteadyClockTimePoint& out_time_point,
                                                            LocationName& location_name) {
    R_TRY(m_time_zone.GetLocationName(location_name));
    R_RETURN(m_time_zone.GetTimePoint(out_time_point));
}

Result TimeZoneService::SetDeviceLocationNameWithTimeZoneRule(LocationName& location_name,
                                                              std::span<const u8> binary) {
    R_UNLESS(m_can_write_timezone_device_location, ResultPermissionDenied);
    R_TRY(m_time_zone.ParseBinary(location_name, binary));

    SteadyClockTimePoint time_point{};
    R_TRY(m_clock_core.GetCurrentTimePoint(time_point));

    m_time_zone.SetTimePoint(time_point);
    R_SUCCEED();
}

Result TimeZoneService::ParseTimeZoneBinary(Tz::Rule& out_rule, std::span<const u8> binary) {
    R_RETURN(m_time_zone.ParseBinaryInto(out_rule, binary));
}

Result TimeZoneService::ToCalendarTime(CalendarTime& out_calendar_time,
                                       CalendarAdditionalInfo& out_additional_info, s64 time,
                                       Tz::Rule& rule) {
    R_RETURN(m_time_zone.ToCalendarTime(out_calendar_time, out_additional_info, time, rule));
}

Result TimeZoneService::ToCalendarTimeWithMyRule(CalendarTime& out_calendar_time,
                                                 CalendarAdditionalInfo& out_additional_info,
                                                 s64 time) {
    R_RETURN(m_time_zone.ToCalendarTimeWithMyRule(out_calendar_time, out_additional_info, time));
}

Result TimeZoneService::ToPosixTime(u32& out_count, std::span<s64, 2> out_times,
                                    u32 out_times_count, CalendarTime& calendar_time,
                                    Tz::Rule& rule) {
    R_RETURN(m_time_zone.ToPosixTime(out_count, out_times, out_times_count, calendar_time, rule));
}

Result TimeZoneService::ToPosixTimeWithMyRule(u32& out_count, std::span<s64, 2> out_times,
                                              u32 out_times_count, CalendarTime& calendar_time) {
    R_RETURN(
        m_time_zone.ToPosixTimeWithMyRule(out_count, out_times, out_times_count, calendar_time));
}

} // namespace Service::PSC::Time
