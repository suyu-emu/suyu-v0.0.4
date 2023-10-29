// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <span>
#include <vector>

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Tz {
struct Rule;
}

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::PSC::Time {
class TimeZoneService;
}

namespace Service::Glue::Time {
class FileTimestampWorker;

class TimeZoneService final : public ServiceFramework<TimeZoneService> {
public:
    explicit TimeZoneService(
        Core::System& system, FileTimestampWorker& file_timestamp_worker,
        bool can_write_timezone_device_location,
        std::shared_ptr<Service::PSC::Time::TimeZoneService> time_zone_service);

    ~TimeZoneService() override;

    Result GetDeviceLocationName(Service::PSC::Time::LocationName& out_location_name);
    Result SetDeviceLocation(Service::PSC::Time::LocationName& location_name);
    Result GetTotalLocationNameCount(u32& out_count);
    Result LoadLocationNameList(u32& out_count,
                                std::vector<Service::PSC::Time::LocationName>& out_names,
                                size_t max_names, u32 index);
    Result LoadTimeZoneRule(Tz::Rule& out_rule, Service::PSC::Time::LocationName& name);
    Result GetTimeZoneRuleVersion(Service::PSC::Time::RuleVersion& out_rule_version);
    Result GetDeviceLocationNameAndUpdatedTime(
        Service::PSC::Time::SteadyClockTimePoint& out_time_point,
        Service::PSC::Time::LocationName& location_name);
    Result SetDeviceLocationNameWithTimeZoneRule();
    Result GetDeviceLocationNameOperationEventReadableHandle(Kernel::KEvent** out_event);
    Result ToCalendarTime(Service::PSC::Time::CalendarTime& out_calendar_time,
                          Service::PSC::Time::CalendarAdditionalInfo& out_additional_info, s64 time,
                          Tz::Rule& rule);
    Result ToCalendarTimeWithMyRule(Service::PSC::Time::CalendarTime& out_calendar_time,
                                    Service::PSC::Time::CalendarAdditionalInfo& out_additional_info,
                                    s64 time);
    Result ToPosixTime(u32& out_count, std::span<s64, 2> out_times, u32 out_times_count,
                       Service::PSC::Time::CalendarTime& calendar_time, Tz::Rule& rule);
    Result ToPosixTimeWithMyRule(u32& out_count, std::span<s64, 2> out_times, u32 out_times_count,
                                 Service::PSC::Time::CalendarTime& calendar_time);

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
    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;

    bool m_can_write_timezone_device_location;
    FileTimestampWorker& m_file_timestamp_worker;
    std::shared_ptr<Service::PSC::Time::TimeZoneService> m_wrapped_service;
    std::mutex m_mutex;
    bool operation_event_initialized{};
    Service::PSC::Time::OperationEvent m_operation_event;
};

} // namespace Service::Glue::Time
