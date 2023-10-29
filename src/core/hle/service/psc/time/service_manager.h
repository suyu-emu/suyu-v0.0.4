// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <list>
#include <memory>

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KReadableEvent;
}

namespace Service::PSC::Time {
class StaticService;

class ServiceManager final : public ServiceFramework<ServiceManager> {
public:
    explicit ServiceManager(Core::System& system, std::shared_ptr<TimeManager> time,
                            ServerManager* server_manager);
    ~ServiceManager() override = default;

    Result GetStaticServiceAsUser(std::shared_ptr<StaticService>& out_service);
    Result GetStaticServiceAsAdmin(std::shared_ptr<StaticService>& out_service);
    Result GetStaticServiceAsRepair(std::shared_ptr<StaticService>& out_service);
    Result GetStaticServiceAsServiceManager(std::shared_ptr<StaticService>& out_service);
    Result SetupStandardSteadyClockCore(Common::UUID& clock_source_id, s64 rtc_offset,
                                        s64 internal_offset, s64 test_offset,
                                        bool is_rtc_reset_detected);
    Result SetupStandardLocalSystemClockCore(SystemClockContext& context, s64 time);
    Result SetupStandardNetworkSystemClockCore(SystemClockContext& context, s64 accuracy);
    Result SetupStandardUserSystemClockCore(SteadyClockTimePoint& time_point,
                                            bool automatic_correction);
    Result SetupTimeZoneServiceCore(LocationName& name, SteadyClockTimePoint& time_point,
                                    RuleVersion& rule_version, u32 location_count,
                                    std::span<const u8> rule_buffer);
    Result SetupEphemeralNetworkSystemClockCore();
    Result GetStandardLocalClockOperationEvent(Kernel::KEvent** out_event);
    Result GetStandardNetworkClockOperationEventForServiceManager(Kernel::KEvent** out_event);
    Result GetEphemeralNetworkClockOperationEventForServiceManager(Kernel::KEvent** out_event);
    Result GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(Kernel::KEvent** out_event);
    Result SetStandardSteadyClockBaseTime(s64 base_time);
    Result GetClosestAlarmUpdatedEvent(Kernel::KEvent** out_event);
    Result CheckAndSignalAlarms();
    Result GetClosestAlarmInfo(bool& out_is_valid, AlarmInfo& out_info, s64& out_time);

private:
    void CheckAndSetupServicesSAndP();
    void SetupSAndP();
    Result GetStaticService(std::shared_ptr<StaticService>& out_service,
                            StaticServiceSetupInfo setup_info, const char* name);

    void Handle_GetStaticServiceAsUser(HLERequestContext& ctx);
    void Handle_GetStaticServiceAsAdmin(HLERequestContext& ctx);
    void Handle_GetStaticServiceAsRepair(HLERequestContext& ctx);
    void Handle_GetStaticServiceAsServiceManager(HLERequestContext& ctx);
    void Handle_SetupStandardSteadyClockCore(HLERequestContext& ctx);
    void Handle_SetupStandardLocalSystemClockCore(HLERequestContext& ctx);
    void Handle_SetupStandardNetworkSystemClockCore(HLERequestContext& ctx);
    void Handle_SetupStandardUserSystemClockCore(HLERequestContext& ctx);
    void Handle_SetupTimeZoneServiceCore(HLERequestContext& ctx);
    void Handle_SetupEphemeralNetworkSystemClockCore(HLERequestContext& ctx);
    void Handle_GetStandardLocalClockOperationEvent(HLERequestContext& ctx);
    void Handle_GetStandardNetworkClockOperationEventForServiceManager(HLERequestContext& ctx);
    void Handle_GetEphemeralNetworkClockOperationEventForServiceManager(HLERequestContext& ctx);
    void Handle_GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(HLERequestContext& ctx);
    void Handle_SetStandardSteadyClockBaseTime(HLERequestContext& ctx);
    void Handle_GetClosestAlarmUpdatedEvent(HLERequestContext& ctx);
    void Handle_CheckAndSignalAlarms(HLERequestContext& ctx);
    void Handle_GetClosestAlarmInfo(HLERequestContext& ctx);

    Core::System& m_system;
    std::shared_ptr<TimeManager> m_time;
    ServerManager& m_server_manager;
    bool m_is_s_and_p_setup{};
    StandardLocalSystemClockCore& m_local_system_clock;
    StandardUserSystemClockCore& m_user_system_clock;
    StandardNetworkSystemClockCore& m_network_system_clock;
    StandardSteadyClockCore& m_steady_clock;
    TimeZone& m_time_zone;
    EphemeralNetworkSystemClockCore& m_ephemeral_network_clock;
    SharedMemory& m_shared_memory;
    Alarms& m_alarms;
    LocalSystemClockContextWriter& m_local_system_context_writer;
    NetworkSystemClockContextWriter& m_network_system_context_writer;
    EphemeralNetworkSystemClockContextWriter& m_ephemeral_system_context_writer;
    OperationEvent m_local_operation;
    OperationEvent m_network_operation;
    OperationEvent m_ephemeral_operation;
};

} // namespace Service::PSC::Time
