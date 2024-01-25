// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "core/core.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/time_zone.h"
#include "core/hle/service/glue/time/time_zone_binary.h"
#include "core/hle/service/psc/time/time_zone_service.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {
namespace {
static std::mutex g_list_mutex;
static Common::IntrusiveListBaseTraits<Service::PSC::Time::OperationEvent>::ListType g_list_nodes{};
} // namespace

TimeZoneService::TimeZoneService(
    Core::System& system_, FileTimestampWorker& file_timestamp_worker,
    bool can_write_timezone_device_location,
    std::shared_ptr<Service::PSC::Time::TimeZoneService> time_zone_service)
    : ServiceFramework{system_, "ITimeZoneService"}, m_system{system},
      m_can_write_timezone_device_location{can_write_timezone_device_location},
      m_file_timestamp_worker{file_timestamp_worker},
      m_wrapped_service{std::move(time_zone_service)}, m_operation_event{m_system} {
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

    g_list_nodes.clear();
    m_set_sys =
        m_system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);
}

TimeZoneService::~TimeZoneService() = default;

void TimeZoneService::Handle_GetDeviceLocationName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Service::PSC::Time::LocationName name{};
    auto res = GetDeviceLocationName(name);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(Service::PSC::Time::LocationName) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<Service::PSC::Time::LocationName>(name);
}

void TimeZoneService::Handle_SetDeviceLocationName(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto name{rp.PopRaw<Service::PSC::Time::LocationName>()};

    auto res = SetDeviceLocation(name);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
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

    IPC::RequestParser rp{ctx};
    auto index{rp.Pop<u32>()};

    auto max_names{ctx.GetWriteBufferSize() / sizeof(Service::PSC::Time::LocationName)};

    std::vector<Service::PSC::Time::LocationName> names{};
    u32 count{};
    auto res = LoadLocationNameList(count, names, max_names, index);

    ctx.WriteBuffer(names);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(count);
}

void TimeZoneService::Handle_LoadTimeZoneRule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto name{rp.PopRaw<Service::PSC::Time::LocationName>()};

    Tz::Rule rule{};
    auto res = LoadTimeZoneRule(rule, name);

    ctx.WriteBuffer(rule);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void TimeZoneService::Handle_GetTimeZoneRuleVersion(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Service::PSC::Time::RuleVersion rule_version{};
    auto res = GetTimeZoneRuleVersion(rule_version);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(Service::PSC::Time::RuleVersion) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<Service::PSC::Time::RuleVersion>(rule_version);
}

void TimeZoneService::Handle_GetDeviceLocationNameAndUpdatedTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Service::PSC::Time::LocationName name{};
    Service::PSC::Time::SteadyClockTimePoint time_point{};
    auto res = GetDeviceLocationNameAndUpdatedTime(time_point, name);

    IPC::ResponseBuilder rb{ctx,
                            2 + (sizeof(Service::PSC::Time::LocationName) / sizeof(u32)) +
                                (sizeof(Service::PSC::Time::SteadyClockTimePoint) / sizeof(u32))};
    rb.Push(res);
    rb.PushRaw<Service::PSC::Time::LocationName>(name);
    rb.PushRaw<Service::PSC::Time::SteadyClockTimePoint>(time_point);
}

void TimeZoneService::Handle_SetDeviceLocationNameWithTimeZoneRule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    auto res = SetDeviceLocationNameWithTimeZoneRule();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void TimeZoneService::Handle_ParseTimeZoneBinary(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(Service::PSC::Time::ResultNotImplemented);
}

void TimeZoneService::Handle_GetDeviceLocationNameOperationEventReadableHandle(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetDeviceLocationNameOperationEventReadableHandle(&event);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(event->GetReadableEvent());
}

void TimeZoneService::Handle_ToCalendarTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto time{rp.Pop<s64>()};

    auto rule_buffer{ctx.ReadBuffer()};
    Tz::Rule rule{};
    std::memcpy(&rule, rule_buffer.data(), sizeof(Tz::Rule));

    Service::PSC::Time::CalendarTime calendar_time{};
    Service::PSC::Time::CalendarAdditionalInfo additional_info{};
    auto res = ToCalendarTime(calendar_time, additional_info, time, rule);

    IPC::ResponseBuilder rb{ctx,
                            2 + (sizeof(Service::PSC::Time::CalendarTime) / sizeof(u32)) +
                                (sizeof(Service::PSC::Time::CalendarAdditionalInfo) / sizeof(u32))};
    rb.Push(res);
    rb.PushRaw<Service::PSC::Time::CalendarTime>(calendar_time);
    rb.PushRaw<Service::PSC::Time::CalendarAdditionalInfo>(additional_info);
}

void TimeZoneService::Handle_ToCalendarTimeWithMyRule(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto time{rp.Pop<s64>()};

    LOG_DEBUG(Service_Time, "called. time={}", time);

    Service::PSC::Time::CalendarTime calendar_time{};
    Service::PSC::Time::CalendarAdditionalInfo additional_info{};
    auto res = ToCalendarTimeWithMyRule(calendar_time, additional_info, time);

    IPC::ResponseBuilder rb{ctx,
                            2 + (sizeof(Service::PSC::Time::CalendarTime) / sizeof(u32)) +
                                (sizeof(Service::PSC::Time::CalendarAdditionalInfo) / sizeof(u32))};
    rb.Push(res);
    rb.PushRaw<Service::PSC::Time::CalendarTime>(calendar_time);
    rb.PushRaw<Service::PSC::Time::CalendarAdditionalInfo>(additional_info);
}

void TimeZoneService::Handle_ToPosixTime(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    auto calendar{rp.PopRaw<Service::PSC::Time::CalendarTime>()};

    LOG_DEBUG(Service_Time, "called. calendar year {} month {} day {} hour {} minute {} second {}",
              calendar.year, calendar.month, calendar.day, calendar.hour, calendar.minute,
              calendar.second);

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
    auto calendar{rp.PopRaw<Service::PSC::Time::CalendarTime>()};

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

Result TimeZoneService::GetDeviceLocationName(Service::PSC::Time::LocationName& out_location_name) {
    R_RETURN(m_wrapped_service->GetDeviceLocationName(out_location_name));
}

Result TimeZoneService::SetDeviceLocation(Service::PSC::Time::LocationName& location_name) {
    R_UNLESS(m_can_write_timezone_device_location, Service::PSC::Time::ResultPermissionDenied);
    R_UNLESS(IsTimeZoneBinaryValid(location_name), Service::PSC::Time::ResultTimeZoneNotFound);

    std::scoped_lock l{m_mutex};

    std::span<const u8> binary{};
    size_t binary_size{};
    R_TRY(GetTimeZoneRule(binary, binary_size, location_name))

    R_TRY(m_wrapped_service->SetDeviceLocationNameWithTimeZoneRule(location_name, binary));

    m_file_timestamp_worker.SetFilesystemPosixTime();

    Service::PSC::Time::SteadyClockTimePoint time_point{};
    Service::PSC::Time::LocationName name{};
    R_TRY(m_wrapped_service->GetDeviceLocationNameAndUpdatedTime(time_point, name));

    m_set_sys->SetDeviceTimeZoneLocationName(name);
    m_set_sys->SetDeviceTimeZoneLocationUpdatedTime(time_point);

    std::scoped_lock m{g_list_mutex};
    for (auto& operation_event : g_list_nodes) {
        operation_event.m_event->Signal();
    }
    R_SUCCEED();
}

Result TimeZoneService::GetTotalLocationNameCount(u32& out_count) {
    R_RETURN(m_wrapped_service->GetTotalLocationNameCount(out_count));
}

Result TimeZoneService::LoadLocationNameList(
    u32& out_count, std::vector<Service::PSC::Time::LocationName>& out_names, size_t max_names,
    u32 index) {
    std::scoped_lock l{m_mutex};
    R_RETURN(GetTimeZoneLocationList(out_count, out_names, max_names, index));
}

Result TimeZoneService::LoadTimeZoneRule(Tz::Rule& out_rule,
                                         Service::PSC::Time::LocationName& name) {
    std::scoped_lock l{m_mutex};
    std::span<const u8> binary{};
    size_t binary_size{};
    R_TRY(GetTimeZoneRule(binary, binary_size, name))
    R_RETURN(m_wrapped_service->ParseTimeZoneBinary(out_rule, binary));
}

Result TimeZoneService::GetTimeZoneRuleVersion(Service::PSC::Time::RuleVersion& out_rule_version) {
    R_RETURN(m_wrapped_service->GetTimeZoneRuleVersion(out_rule_version));
}

Result TimeZoneService::GetDeviceLocationNameAndUpdatedTime(
    Service::PSC::Time::SteadyClockTimePoint& out_time_point,
    Service::PSC::Time::LocationName& location_name) {
    R_RETURN(m_wrapped_service->GetDeviceLocationNameAndUpdatedTime(out_time_point, location_name));
}

Result TimeZoneService::SetDeviceLocationNameWithTimeZoneRule() {
    R_UNLESS(m_can_write_timezone_device_location, Service::PSC::Time::ResultPermissionDenied);
    R_RETURN(Service::PSC::Time::ResultNotImplemented);
}

Result TimeZoneService::GetDeviceLocationNameOperationEventReadableHandle(
    Kernel::KEvent** out_event) {
    if (!operation_event_initialized) {
        operation_event_initialized = false;

        m_operation_event.m_ctx.CloseEvent(m_operation_event.m_event);
        m_operation_event.m_event =
            m_operation_event.m_ctx.CreateEvent("Psc:TimeZoneService:OperationEvent");
        operation_event_initialized = true;
        std::scoped_lock l{m_mutex};
        g_list_nodes.push_back(m_operation_event);
    }

    *out_event = m_operation_event.m_event;
    R_SUCCEED();
}

Result TimeZoneService::ToCalendarTime(
    Service::PSC::Time::CalendarTime& out_calendar_time,
    Service::PSC::Time::CalendarAdditionalInfo& out_additional_info, s64 time, Tz::Rule& rule) {
    R_RETURN(m_wrapped_service->ToCalendarTime(out_calendar_time, out_additional_info, time, rule));
}

Result TimeZoneService::ToCalendarTimeWithMyRule(
    Service::PSC::Time::CalendarTime& out_calendar_time,
    Service::PSC::Time::CalendarAdditionalInfo& out_additional_info, s64 time) {
    R_RETURN(
        m_wrapped_service->ToCalendarTimeWithMyRule(out_calendar_time, out_additional_info, time));
}

Result TimeZoneService::ToPosixTime(u32& out_count, std::span<s64, 2> out_times,
                                    u32 out_times_count,
                                    Service::PSC::Time::CalendarTime& calendar_time,
                                    Tz::Rule& rule) {
    R_RETURN(
        m_wrapped_service->ToPosixTime(out_count, out_times, out_times_count, calendar_time, rule));
}

Result TimeZoneService::ToPosixTimeWithMyRule(u32& out_count, std::span<s64, 2> out_times,
                                              u32 out_times_count,
                                              Service::PSC::Time::CalendarTime& calendar_time) {
    R_RETURN(m_wrapped_service->ToPosixTimeWithMyRule(out_count, out_times, out_times_count,
                                                      calendar_time));
}

} // namespace Service::Glue::Time
