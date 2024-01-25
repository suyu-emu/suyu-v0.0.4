// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/psc/time/power_state_service.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"

namespace Service::PSC::Time {

ServiceManager::ServiceManager(Core::System& system_, std::shared_ptr<TimeManager> time,
                               ServerManager* server_manager)
    : ServiceFramework{system_, "time:m"}, m_system{system}, m_time{std::move(time)},
      m_server_manager{*server_manager},
      m_local_system_clock{m_time->m_standard_local_system_clock},
      m_user_system_clock{m_time->m_standard_user_system_clock},
      m_network_system_clock{m_time->m_standard_network_system_clock},
      m_steady_clock{m_time->m_standard_steady_clock}, m_time_zone{m_time->m_time_zone},
      m_ephemeral_network_clock{m_time->m_ephemeral_network_clock},
      m_shared_memory{m_time->m_shared_memory}, m_alarms{m_time->m_alarms},
      m_local_system_context_writer{m_time->m_local_system_clock_context_writer},
      m_network_system_context_writer{m_time->m_network_system_clock_context_writer},
      m_ephemeral_system_context_writer{m_time->m_ephemeral_network_clock_context_writer},
      m_local_operation{m_system}, m_network_operation{m_system}, m_ephemeral_operation{m_system} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0,   &ServiceManager::Handle_GetStaticServiceAsUser, "GetStaticServiceAsUser"},
        {5,   &ServiceManager::Handle_GetStaticServiceAsAdmin, "GetStaticServiceAsAdmin"},
        {6,   &ServiceManager::Handle_GetStaticServiceAsRepair, "GetStaticServiceAsRepair"},
        {9,   &ServiceManager::Handle_GetStaticServiceAsServiceManager, "GetStaticServiceAsServiceManager"},
        {10,  &ServiceManager::Handle_SetupStandardSteadyClockCore, "SetupStandardSteadyClockCore"},
        {11,  &ServiceManager::Handle_SetupStandardLocalSystemClockCore, "SetupStandardLocalSystemClockCore"},
        {12,  &ServiceManager::Handle_SetupStandardNetworkSystemClockCore, "SetupStandardNetworkSystemClockCore"},
        {13,  &ServiceManager::Handle_SetupStandardUserSystemClockCore, "SetupStandardUserSystemClockCore"},
        {14,  &ServiceManager::Handle_SetupTimeZoneServiceCore, "SetupTimeZoneServiceCore"},
        {15,  &ServiceManager::Handle_SetupEphemeralNetworkSystemClockCore, "SetupEphemeralNetworkSystemClockCore"},
        {50,  &ServiceManager::Handle_GetStandardLocalClockOperationEvent, "GetStandardLocalClockOperationEvent"},
        {51,  &ServiceManager::Handle_GetStandardNetworkClockOperationEventForServiceManager, "GetStandardNetworkClockOperationEventForServiceManager"},
        {52,  &ServiceManager::Handle_GetEphemeralNetworkClockOperationEventForServiceManager, "GetEphemeralNetworkClockOperationEventForServiceManager"},
        {60,  &ServiceManager::Handle_GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent, "GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent"},
        {100, &ServiceManager::Handle_SetStandardSteadyClockBaseTime, "SetStandardSteadyClockBaseTime"},
        {200, &ServiceManager::Handle_GetClosestAlarmUpdatedEvent, "GetClosestAlarmUpdatedEvent"},
        {201, &ServiceManager::Handle_CheckAndSignalAlarms, "CheckAndSignalAlarms"},
        {202, &ServiceManager::Handle_GetClosestAlarmInfo, "GetClosestAlarmInfo "},
    };
    // clang-format on
    RegisterHandlers(functions);

    m_local_system_context_writer.Link(m_local_operation);
    m_network_system_context_writer.Link(m_network_operation);
    m_ephemeral_system_context_writer.Link(m_ephemeral_operation);
}

void ServiceManager::SetupSAndP() {
    if (!m_is_s_and_p_setup) {
        m_is_s_and_p_setup = true;
        m_server_manager.RegisterNamedService(
            "time:s", std::make_shared<StaticService>(
                          m_system, StaticServiceSetupInfo{0, 0, 1, 0, 0, 0}, m_time, "time:s"));
        m_server_manager.RegisterNamedService("time:p",
                                              std::make_shared<IPowerStateRequestHandler>(
                                                  m_system, m_time->m_power_state_request_manager));
    }
}

void ServiceManager::CheckAndSetupServicesSAndP() {
    if (m_local_system_clock.IsInitialized() && m_user_system_clock.IsInitialized() &&
        m_network_system_clock.IsInitialized() && m_steady_clock.IsInitialized() &&
        m_time_zone.IsInitialized() && m_ephemeral_network_clock.IsInitialized()) {
        SetupSAndP();
    }
}

void ServiceManager::Handle_GetStaticServiceAsUser(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<StaticService> service{};
    auto res = GetStaticServiceAsUser(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<StaticService>(std::move(service));
}

void ServiceManager::Handle_GetStaticServiceAsAdmin(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<StaticService> service{};
    auto res = GetStaticServiceAsAdmin(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<StaticService>(std::move(service));
}

void ServiceManager::Handle_GetStaticServiceAsRepair(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<StaticService> service{};
    auto res = GetStaticServiceAsRepair(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<StaticService>(std::move(service));
}

void ServiceManager::Handle_GetStaticServiceAsServiceManager(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<StaticService> service{};
    auto res = GetStaticServiceAsServiceManager(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<StaticService>(std::move(service));
}

void ServiceManager::Handle_SetupStandardSteadyClockCore(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    struct Parameters {
        bool reset_detected;
        Common::UUID clock_source_id;
        s64 rtc_offset;
        s64 internal_offset;
        s64 test_offset;
    };
    static_assert(sizeof(Parameters) == 0x30);

    IPC::RequestParser rp{ctx};
    auto params{rp.PopRaw<Parameters>()};

    auto res = SetupStandardSteadyClockCore(params.clock_source_id, params.rtc_offset,
                                            params.internal_offset, params.test_offset,
                                            params.reset_detected);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_SetupStandardLocalSystemClockCore(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<SystemClockContext>()};
    auto time{rp.Pop<s64>()};

    auto res = SetupStandardLocalSystemClockCore(context, time);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_SetupStandardNetworkSystemClockCore(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<SystemClockContext>()};
    auto accuracy{rp.Pop<s64>()};

    auto res = SetupStandardNetworkSystemClockCore(context, accuracy);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_SetupStandardUserSystemClockCore(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    struct Parameters {
        bool automatic_correction;
        SteadyClockTimePoint time_point;
    };
    static_assert(sizeof(Parameters) == 0x20);

    IPC::RequestParser rp{ctx};
    auto params{rp.PopRaw<Parameters>()};

    auto res = SetupStandardUserSystemClockCore(params.time_point, params.automatic_correction);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_SetupTimeZoneServiceCore(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    struct Parameters {
        u32 location_count;
        LocationName name;
        SteadyClockTimePoint time_point;
        RuleVersion rule_version;
    };
    static_assert(sizeof(Parameters) == 0x50);

    IPC::RequestParser rp{ctx};
    auto params{rp.PopRaw<Parameters>()};

    auto rule_buffer{ctx.ReadBuffer()};

    auto res = SetupTimeZoneServiceCore(params.name, params.time_point, params.rule_version,
                                        params.location_count, rule_buffer);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_SetupEphemeralNetworkSystemClockCore(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    auto res = SetupEphemeralNetworkSystemClockCore();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_GetStandardLocalClockOperationEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetStandardLocalClockOperationEvent(&event);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(event->GetReadableEvent());
}

void ServiceManager::Handle_GetStandardNetworkClockOperationEventForServiceManager(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetStandardNetworkClockOperationEventForServiceManager(&event);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(event);
}

void ServiceManager::Handle_GetEphemeralNetworkClockOperationEventForServiceManager(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetEphemeralNetworkClockOperationEventForServiceManager(&event);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(event);
}

void ServiceManager::Handle_GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(&event);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(event);
}

void ServiceManager::Handle_SetStandardSteadyClockBaseTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto base_time{rp.Pop<s64>()};

    auto res = SetStandardSteadyClockBaseTime(base_time);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_GetClosestAlarmUpdatedEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetClosestAlarmUpdatedEvent(&event);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.PushCopyObjects(event->GetReadableEvent());
}

void ServiceManager::Handle_CheckAndSignalAlarms(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    auto res = CheckAndSignalAlarms();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void ServiceManager::Handle_GetClosestAlarmInfo(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    AlarmInfo alarm_info{};
    bool is_valid{};
    s64 time{};
    auto res = GetClosestAlarmInfo(is_valid, alarm_info, time);

    struct OutParameters {
        bool is_valid;
        AlarmInfo alarm_info;
        s64 time;
    };
    static_assert(sizeof(OutParameters) == 0x20);

    OutParameters out_params{
        .is_valid = is_valid,
        .alarm_info = alarm_info,
        .time = time,
    };

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(OutParameters) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<OutParameters>(out_params);
}

// =============================== Implementations ===========================

Result ServiceManager::GetStaticService(std::shared_ptr<StaticService>& out_service,
                                        StaticServiceSetupInfo setup_info, const char* name) {
    out_service = std::make_shared<StaticService>(m_system, setup_info, m_time, name);
    R_SUCCEED();
}

Result ServiceManager::GetStaticServiceAsUser(std::shared_ptr<StaticService>& out_service) {
    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{0, 0, 0, 0, 0, 0}, "time:u"));
}

Result ServiceManager::GetStaticServiceAsAdmin(std::shared_ptr<StaticService>& out_service) {
    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{1, 1, 0, 1, 0, 0}, "time:a"));
}

Result ServiceManager::GetStaticServiceAsRepair(std::shared_ptr<StaticService>& out_service) {
    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{0, 0, 0, 0, 1, 0}, "time:r"));
}

Result ServiceManager::GetStaticServiceAsServiceManager(
    std::shared_ptr<StaticService>& out_service) {
    R_RETURN(GetStaticService(out_service, StaticServiceSetupInfo{1, 1, 1, 1, 1, 0}, "time:sm"));
}

Result ServiceManager::SetupStandardSteadyClockCore(Common::UUID& clock_source_id, s64 rtc_offset,
                                                    s64 internal_offset, s64 test_offset,
                                                    bool is_rtc_reset_detected) {
    m_steady_clock.Initialize(clock_source_id, rtc_offset, internal_offset, test_offset,
                              is_rtc_reset_detected);
    auto time = m_steady_clock.GetRawTime();
    auto ticks = m_system.CoreTiming().GetClockTicks();
    auto boot_time = time - ConvertToTimeSpan(ticks).count();
    m_shared_memory.SetSteadyClockTimePoint(clock_source_id, boot_time);
    m_steady_clock.SetContinuousAdjustment(clock_source_id, boot_time);

    ContinuousAdjustmentTimePoint time_point{};
    m_steady_clock.GetContinuousAdjustment(time_point);
    m_shared_memory.SetContinuousAdjustment(time_point);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupStandardLocalSystemClockCore(SystemClockContext& context, s64 time) {
    m_local_system_clock.SetContextWriter(m_local_system_context_writer);
    m_local_system_clock.Initialize(context, time);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupStandardNetworkSystemClockCore(SystemClockContext& context,
                                                           s64 accuracy) {
    // TODO this is a hack! The network clock should be updated independently, from the ntc service
    // and maybe elsewhere. We do not do that, so fix the clock to the local clock on first boot
    // to avoid it being stuck at 0.
    if (context == Service::PSC::Time::SystemClockContext{}) {
        m_local_system_clock.GetContext(context);
    }

    m_network_system_clock.SetContextWriter(m_network_system_context_writer);
    m_network_system_clock.Initialize(context, accuracy);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupStandardUserSystemClockCore(SteadyClockTimePoint& time_point,
                                                        bool automatic_correction) {
    // TODO this is a hack! The user clock should be updated independently, from the ntc service
    // and maybe elsewhere. We do not do that, so fix the clock to the local clock on first boot
    // to avoid it being stuck at 0.
    if (time_point == Service::PSC::Time::SteadyClockTimePoint{}) {
        m_local_system_clock.GetCurrentTimePoint(time_point);
    }

    m_user_system_clock.SetAutomaticCorrection(automatic_correction);
    m_user_system_clock.SetTimePointAndSignal(time_point);
    m_user_system_clock.SetInitialized();
    m_shared_memory.SetAutomaticCorrection(automatic_correction);

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupTimeZoneServiceCore(LocationName& name,
                                                SteadyClockTimePoint& time_point,
                                                RuleVersion& rule_version, u32 location_count,
                                                std::span<const u8> rule_buffer) {
    if (m_time_zone.ParseBinary(name, rule_buffer) != ResultSuccess) {
        LOG_ERROR(Service_Time, "Failed to parse time zone binary!");
    }

    m_time_zone.SetTimePoint(time_point);
    m_time_zone.SetTotalLocationNameCount(location_count);
    m_time_zone.SetRuleVersion(rule_version);
    m_time_zone.SetInitialized();

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::SetupEphemeralNetworkSystemClockCore() {
    m_ephemeral_network_clock.SetContextWriter(m_ephemeral_system_context_writer);
    m_ephemeral_network_clock.SetInitialized();

    CheckAndSetupServicesSAndP();
    R_SUCCEED();
}

Result ServiceManager::GetStandardLocalClockOperationEvent(Kernel::KEvent** out_event) {
    *out_event = m_local_operation.m_event;
    R_SUCCEED();
}

Result ServiceManager::GetStandardNetworkClockOperationEventForServiceManager(
    Kernel::KEvent** out_event) {
    *out_event = m_network_operation.m_event;
    R_SUCCEED();
}

Result ServiceManager::GetEphemeralNetworkClockOperationEventForServiceManager(
    Kernel::KEvent** out_event) {
    *out_event = m_ephemeral_operation.m_event;
    R_SUCCEED();
}

Result ServiceManager::GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(
    Kernel::KEvent** out_event) {
    *out_event = &m_user_system_clock.GetEvent();
    R_SUCCEED();
}

Result ServiceManager::SetStandardSteadyClockBaseTime(s64 base_time) {
    m_steady_clock.SetRtcOffset(base_time);
    auto time = m_steady_clock.GetRawTime();
    auto ticks = m_system.CoreTiming().GetClockTicks();
    auto diff = time - ConvertToTimeSpan(ticks).count();
    m_shared_memory.UpdateBaseTime(diff);
    m_steady_clock.UpdateContinuousAdjustmentTime(diff);

    ContinuousAdjustmentTimePoint time_point{};
    m_steady_clock.GetContinuousAdjustment(time_point);
    m_shared_memory.SetContinuousAdjustment(time_point);
    R_SUCCEED();
}

Result ServiceManager::GetClosestAlarmUpdatedEvent(Kernel::KEvent** out_event) {
    *out_event = &m_alarms.GetEvent();
    R_SUCCEED();
}

Result ServiceManager::CheckAndSignalAlarms() {
    m_alarms.CheckAndSignal();
    R_SUCCEED();
}

Result ServiceManager::GetClosestAlarmInfo(bool& out_is_valid, AlarmInfo& out_info, s64& out_time) {
    Alarm* alarm{nullptr};
    out_is_valid = m_alarms.GetClosestAlarm(&alarm);
    if (out_is_valid) {
        out_info = {
            .alert_time = alarm->GetAlertTime(),
            .priority = alarm->GetPriority(),
        };
        out_time = m_alarms.GetRawTime();
    }
    R_SUCCEED();
}

} // namespace Service::PSC::Time
