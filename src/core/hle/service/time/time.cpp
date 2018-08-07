// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <ctime>
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/service/time/interface.h"
#include "core/hle/service/time/time.h"

namespace Service::Time {

class ISystemClock final : public ServiceFramework<ISystemClock> {
public:
    ISystemClock() : ServiceFramework("ISystemClock") {
        static const FunctionInfo functions[] = {
            {0, &ISystemClock::GetCurrentTime, "GetCurrentTime"},
            {1, nullptr, "SetCurrentTime"},
            {2, &ISystemClock::GetSystemClockContext, "GetSystemClockContext"},
            {3, nullptr, "SetSystemClockContext"},

        };
        RegisterHandlers(functions);
    }

private:
    void GetCurrentTime(Kernel::HLERequestContext& ctx) {
        const s64 time_since_epoch{std::chrono::duration_cast<std::chrono::seconds>(
                                       std::chrono::system_clock::now().time_since_epoch())
                                       .count()};
        LOG_DEBUG(Service_Time, "called");
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u64>(time_since_epoch);
    }

    void GetSystemClockContext(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_Time, "(STUBBED) called");
        SystemClockContext system_clock_ontext{};
        IPC::ResponseBuilder rb{ctx, (sizeof(SystemClockContext) / 4) + 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(system_clock_ontext);
    }
};

class ISteadyClock final : public ServiceFramework<ISteadyClock> {
public:
    ISteadyClock() : ServiceFramework("ISteadyClock") {
        static const FunctionInfo functions[] = {
            {0, &ISteadyClock::GetCurrentTimePoint, "GetCurrentTimePoint"},
        };
        RegisterHandlers(functions);
    }

private:
    void GetCurrentTimePoint(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");
        SteadyClockTimePoint steady_clock_time_point{
            CoreTiming::cyclesToMs(CoreTiming::GetTicks()) / 1000};
        IPC::ResponseBuilder rb{ctx, (sizeof(SteadyClockTimePoint) / 4) + 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(steady_clock_time_point);
    }
};

class ITimeZoneService final : public ServiceFramework<ITimeZoneService> {
public:
    ITimeZoneService() : ServiceFramework("ITimeZoneService") {
        static const FunctionInfo functions[] = {
            {0, &ITimeZoneService::GetDeviceLocationName, "GetDeviceLocationName"},
            {1, nullptr, "SetDeviceLocationName"},
            {2, &ITimeZoneService::GetTotalLocationNameCount, "GetTotalLocationNameCount"},
            {3, nullptr, "LoadLocationNameList"},
            {4, &ITimeZoneService::LoadTimeZoneRule, "LoadTimeZoneRule"},
            {5, nullptr, "GetTimeZoneRuleVersion"},
            {100, &ITimeZoneService::ToCalendarTime, "ToCalendarTime"},
            {101, &ITimeZoneService::ToCalendarTimeWithMyRule, "ToCalendarTimeWithMyRule"},
            {201, nullptr, "ToPosixTime"},
            {202, nullptr, "ToPosixTimeWithMyRule"},
        };
        RegisterHandlers(functions);
    }

private:
    LocationName location_name{"UTC"};
    TimeZoneRule my_time_zone_rule{};

    void GetDeviceLocationName(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_Time, "called");
        IPC::ResponseBuilder rb{ctx, (sizeof(LocationName) / 4) + 2};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(location_name);
    }

    void GetTotalLocationNameCount(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_Time, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0);
    }

    void LoadTimeZoneRule(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_Time, "(STUBBED) called");

        ctx.WriteBuffer(&my_time_zone_rule, sizeof(TimeZoneRule));

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void ToCalendarTime(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 posix_time = rp.Pop<u64>();

        LOG_WARNING(Service_Time, "(STUBBED) called, posix_time=0x{:016X}", posix_time);

        TimeZoneRule time_zone_rule{};
        auto buffer = ctx.ReadBuffer();
        std::memcpy(&time_zone_rule, buffer.data(), buffer.size());

        CalendarTime calendar_time{2018, 1, 1, 0, 0, 0};
        CalendarAdditionalInfo additional_info{};

        PosixToCalendar(posix_time, calendar_time, additional_info, time_zone_rule);

        IPC::ResponseBuilder rb{ctx, 10};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(calendar_time);
        rb.PushRaw(additional_info);
    }

    void ToCalendarTimeWithMyRule(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const u64 posix_time = rp.Pop<u64>();

        LOG_WARNING(Service_Time, "(STUBBED) called, posix_time=0x{:016X}", posix_time);

        CalendarTime calendar_time{2018, 1, 1, 0, 0, 0};
        CalendarAdditionalInfo additional_info{};

        PosixToCalendar(posix_time, calendar_time, additional_info, my_time_zone_rule);

        IPC::ResponseBuilder rb{ctx, 10};
        rb.Push(RESULT_SUCCESS);
        rb.PushRaw(calendar_time);
        rb.PushRaw(additional_info);
    }

    void PosixToCalendar(u64 posix_time, CalendarTime& calendar_time,
                         CalendarAdditionalInfo& additional_info, const TimeZoneRule& /*rule*/) {
        std::time_t t(posix_time);
        std::tm* tm = std::localtime(&t);
        if (!tm) {
            return;
        }
        calendar_time.year = tm->tm_year + 1900;
        calendar_time.month = tm->tm_mon + 1;
        calendar_time.day = tm->tm_mday;
        calendar_time.hour = tm->tm_hour;
        calendar_time.minute = tm->tm_min;
        calendar_time.second = tm->tm_sec;

        additional_info.day_of_week = tm->tm_wday;
        additional_info.day_of_year = tm->tm_yday;
        std::memcpy(additional_info.name.data(), "UTC", sizeof("UTC"));
        additional_info.utc_offset = 0;
    }
};

void Module::Interface::GetStandardUserSystemClock(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISystemClock>();
    LOG_DEBUG(Service_Time, "called");
}

void Module::Interface::GetStandardNetworkSystemClock(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISystemClock>();
    LOG_DEBUG(Service_Time, "called");
}

void Module::Interface::GetStandardSteadyClock(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISteadyClock>();
    LOG_DEBUG(Service_Time, "called");
}

void Module::Interface::GetTimeZoneService(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ITimeZoneService>();
    LOG_DEBUG(Service_Time, "called");
}

void Module::Interface::GetStandardLocalSystemClock(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISystemClock>();
    LOG_DEBUG(Service_Time, "called");
}

Module::Interface::Interface(std::shared_ptr<Module> time, const char* name)
    : ServiceFramework(name), time(std::move(time)) {}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto time = std::make_shared<Module>();
    std::make_shared<Time>(time, "time:a")->InstallAsService(service_manager);
    std::make_shared<Time>(time, "time:s")->InstallAsService(service_manager);
    std::make_shared<Time>(time, "time:u")->InstallAsService(service_manager);
}

} // namespace Service::Time
