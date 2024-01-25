// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/service/glue/time/manager.h"
#include "core/hle/service/glue/time/time_zone.h"
#include "core/hle/service/psc/time/common.h"

namespace Core {
class System;
}

namespace Service::Set {
class ISystemSettingsServer;
}

namespace Service::PSC::Time {
class StaticService;
class SystemClock;
class SteadyClock;
class TimeZoneService;
class ServiceManager;
} // namespace Service::PSC::Time

namespace Service::Glue::Time {
class FileTimestampWorker;
class StandardSteadyClockResource;

class StaticService final : public ServiceFramework<StaticService> {
public:
    explicit StaticService(Core::System& system,
                           Service::PSC::Time::StaticServiceSetupInfo setup_info,
                           std::shared_ptr<TimeManager> time, const char* name);

    ~StaticService() override = default;

    Result GetStandardUserSystemClock(
        std::shared_ptr<Service::PSC::Time::SystemClock>& out_service);
    Result GetStandardNetworkSystemClock(
        std::shared_ptr<Service::PSC::Time::SystemClock>& out_service);
    Result GetStandardSteadyClock(std::shared_ptr<Service::PSC::Time::SteadyClock>& out_service);
    Result GetTimeZoneService(std::shared_ptr<TimeZoneService>& out_service);
    Result GetStandardLocalSystemClock(
        std::shared_ptr<Service::PSC::Time::SystemClock>& out_service);
    Result GetEphemeralNetworkSystemClock(
        std::shared_ptr<Service::PSC::Time::SystemClock>& out_service);
    Result GetSharedMemoryNativeHandle(Kernel::KSharedMemory** out_shared_memory);
    Result SetStandardSteadyClockInternalOffset(s64 offset);
    Result GetStandardSteadyClockRtcValue(s64& out_rtc_value);
    Result IsStandardUserSystemClockAutomaticCorrectionEnabled(bool& out_automatic_correction);
    Result SetStandardUserSystemClockAutomaticCorrectionEnabled(bool automatic_correction);
    Result GetStandardUserSystemClockInitialYear(s32& out_year);
    Result IsStandardNetworkSystemClockAccuracySufficient(bool& out_is_sufficient);
    Result GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
        Service::PSC::Time::SteadyClockTimePoint& out_time_point);
    Result CalculateMonotonicSystemClockBaseTimePoint(
        s64& out_time, Service::PSC::Time::SystemClockContext& context);
    Result GetClockSnapshot(Service::PSC::Time::ClockSnapshot& out_snapshot,
                            Service::PSC::Time::TimeType type);
    Result GetClockSnapshotFromSystemClockContext(
        Service::PSC::Time::ClockSnapshot& out_snapshot,
        Service::PSC::Time::SystemClockContext& user_context,
        Service::PSC::Time::SystemClockContext& network_context, Service::PSC::Time::TimeType type);
    Result CalculateStandardUserSystemClockDifferenceByUser(s64& out_time,
                                                            Service::PSC::Time::ClockSnapshot& a,
                                                            Service::PSC::Time::ClockSnapshot& b);
    Result CalculateSpanBetween(s64& out_time, Service::PSC::Time::ClockSnapshot& a,
                                Service::PSC::Time::ClockSnapshot& b);

private:
    Result GetClockSnapshotImpl(Service::PSC::Time::ClockSnapshot& out_snapshot,
                                Service::PSC::Time::SystemClockContext& user_context,
                                Service::PSC::Time::SystemClockContext& network_context,
                                Service::PSC::Time::TimeType type);

    void Handle_GetStandardUserSystemClock(HLERequestContext& ctx);
    void Handle_GetStandardNetworkSystemClock(HLERequestContext& ctx);
    void Handle_GetStandardSteadyClock(HLERequestContext& ctx);
    void Handle_GetTimeZoneService(HLERequestContext& ctx);
    void Handle_GetStandardLocalSystemClock(HLERequestContext& ctx);
    void Handle_GetEphemeralNetworkSystemClock(HLERequestContext& ctx);
    void Handle_GetSharedMemoryNativeHandle(HLERequestContext& ctx);
    void Handle_SetStandardSteadyClockInternalOffset(HLERequestContext& ctx);
    void Handle_GetStandardSteadyClockRtcValue(HLERequestContext& ctx);
    void Handle_IsStandardUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx);
    void Handle_SetStandardUserSystemClockAutomaticCorrectionEnabled(HLERequestContext& ctx);
    void Handle_GetStandardUserSystemClockInitialYear(HLERequestContext& ctx);
    void Handle_IsStandardNetworkSystemClockAccuracySufficient(HLERequestContext& ctx);
    void Handle_GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(HLERequestContext& ctx);
    void Handle_CalculateMonotonicSystemClockBaseTimePoint(HLERequestContext& ctx);
    void Handle_GetClockSnapshot(HLERequestContext& ctx);
    void Handle_GetClockSnapshotFromSystemClockContext(HLERequestContext& ctx);
    void Handle_CalculateStandardUserSystemClockDifferenceByUser(HLERequestContext& ctx);
    void Handle_CalculateSpanBetween(HLERequestContext& ctx);

    Core::System& m_system;

    std::shared_ptr<Service::Set::ISystemSettingsServer> m_set_sys;
    std::shared_ptr<Service::PSC::Time::ServiceManager> m_time_m;
    std::shared_ptr<Service::PSC::Time::StaticService> m_wrapped_service;

    Service::PSC::Time::StaticServiceSetupInfo m_setup_info;
    std::shared_ptr<Service::PSC::Time::StaticService> m_time_sm;
    std::shared_ptr<Service::PSC::Time::TimeZoneService> m_time_zone;
    FileTimestampWorker& m_file_timestamp_worker;
    StandardSteadyClockResource& m_standard_steady_clock_resource;
};
} // namespace Service::Glue::Time
