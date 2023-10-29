// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KSharedMemory;
}

namespace Service::PSC::Time {
class TimeManager;
class StandardLocalSystemClockCore;
class StandardUserSystemClockCore;
class StandardNetworkSystemClockCore;
class TimeZone;
class SystemClock;
class SteadyClock;
class TimeZoneService;
class EphemeralNetworkSystemClockCore;
class SharedMemory;

class StaticService final : public ServiceFramework<StaticService> {
public:
    explicit StaticService(Core::System& system, StaticServiceSetupInfo setup_info,
                           std::shared_ptr<TimeManager> time, const char* name);

    ~StaticService() override = default;

    Result GetStandardUserSystemClock(std::shared_ptr<SystemClock>& out_service);
    Result GetStandardNetworkSystemClock(std::shared_ptr<SystemClock>& out_service);
    Result GetStandardSteadyClock(std::shared_ptr<SteadyClock>& out_service);
    Result GetTimeZoneService(std::shared_ptr<TimeZoneService>& out_service);
    Result GetStandardLocalSystemClock(std::shared_ptr<SystemClock>& out_service);
    Result GetEphemeralNetworkSystemClock(std::shared_ptr<SystemClock>& out_service);
    Result GetSharedMemoryNativeHandle(Kernel::KSharedMemory** out_shared_memory);
    Result IsStandardUserSystemClockAutomaticCorrectionEnabled(bool& out_is_enabled);
    Result SetStandardUserSystemClockAutomaticCorrectionEnabled(bool automatic_correction);
    Result IsStandardNetworkSystemClockAccuracySufficient(bool& out_is_sufficient);
    Result GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
        SteadyClockTimePoint& out_time_point);
    Result CalculateMonotonicSystemClockBaseTimePoint(s64& out_time, SystemClockContext& context);
    Result GetClockSnapshot(ClockSnapshot& out_snapshot, TimeType type);
    Result GetClockSnapshotFromSystemClockContext(ClockSnapshot& out_snapshot,
                                                  SystemClockContext& user_context,
                                                  SystemClockContext& network_context,
                                                  TimeType type);
    Result CalculateStandardUserSystemClockDifferenceByUser(s64& out_time, ClockSnapshot& a,
                                                            ClockSnapshot& b);
    Result CalculateSpanBetween(s64& out_time, ClockSnapshot& a, ClockSnapshot& b);

private:
    Result GetClockSnapshotImpl(ClockSnapshot& out_snapshot, SystemClockContext& user_context,
                                SystemClockContext& network_context, TimeType type);

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
    StaticServiceSetupInfo m_setup_info;
    std::shared_ptr<TimeManager> m_time;
    StandardLocalSystemClockCore& m_local_system_clock;
    StandardUserSystemClockCore& m_user_system_clock;
    StandardNetworkSystemClockCore& m_network_system_clock;
    TimeZone& m_time_zone;
    EphemeralNetworkSystemClockCore& m_ephemeral_network_clock;
    SharedMemory& m_shared_memory;
};

} // namespace Service::PSC::Time
