// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <chrono>

#include "core/core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/static.h"
#include "core/hle/service/psc/time/errors.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/steady_clock.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone_service.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {
namespace {
template <typename T>
T GetSettingsItemValue(std::shared_ptr<Service::Set::ISystemSettingsServer>& set_sys,
                       const char* category, const char* name) {
    std::vector<u8> interval_buf;
    auto res = set_sys->GetSettingsItemValue(interval_buf, category, name);
    ASSERT(res == ResultSuccess);

    T v{};
    std::memcpy(&v, interval_buf.data(), sizeof(T));
    return v;
}
} // namespace

StaticService::StaticService(Core::System& system_,
                             Service::PSC::Time::StaticServiceSetupInfo setup_info,
                             std::shared_ptr<TimeManager> time, const char* name)
    : ServiceFramework{system_, name}, m_system{system_}, m_time_m{time->m_time_m},
      m_setup_info{setup_info}, m_time_sm{time->m_time_sm},
      m_file_timestamp_worker{time->m_file_timestamp_worker}, m_standard_steady_clock_resource{
                                                                  time->m_steady_clock_resource} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0,   &StaticService::Handle_GetStandardUserSystemClock, "GetStandardUserSystemClock"},
            {1,   &StaticService::Handle_GetStandardNetworkSystemClock, "GetStandardNetworkSystemClock"},
            {2,   &StaticService::Handle_GetStandardSteadyClock, "GetStandardSteadyClock"},
            {3,   &StaticService::Handle_GetTimeZoneService, "GetTimeZoneService"},
            {4,   &StaticService::Handle_GetStandardLocalSystemClock, "GetStandardLocalSystemClock"},
            {5,   &StaticService::Handle_GetEphemeralNetworkSystemClock, "GetEphemeralNetworkSystemClock"},
            {20,  &StaticService::Handle_GetSharedMemoryNativeHandle, "GetSharedMemoryNativeHandle"},
            {50,  &StaticService::Handle_SetStandardSteadyClockInternalOffset, "SetStandardSteadyClockInternalOffset"},
            {51,  &StaticService::Handle_GetStandardSteadyClockRtcValue, "GetStandardSteadyClockRtcValue"},
            {100, &StaticService::Handle_IsStandardUserSystemClockAutomaticCorrectionEnabled, "IsStandardUserSystemClockAutomaticCorrectionEnabled"},
            {101, &StaticService::Handle_SetStandardUserSystemClockAutomaticCorrectionEnabled, "SetStandardUserSystemClockAutomaticCorrectionEnabled"},
            {102, &StaticService::Handle_GetStandardUserSystemClockInitialYear, "GetStandardUserSystemClockInitialYear"},
            {200, &StaticService::Handle_IsStandardNetworkSystemClockAccuracySufficient, "IsStandardNetworkSystemClockAccuracySufficient"},
            {201, &StaticService::Handle_GetStandardUserSystemClockAutomaticCorrectionUpdatedTime, "GetStandardUserSystemClockAutomaticCorrectionUpdatedTime"},
            {300, &StaticService::Handle_CalculateMonotonicSystemClockBaseTimePoint, "CalculateMonotonicSystemClockBaseTimePoint"},
            {400, &StaticService::Handle_GetClockSnapshot, "GetClockSnapshot"},
            {401, &StaticService::Handle_GetClockSnapshotFromSystemClockContext, "GetClockSnapshotFromSystemClockContext"},
            {500, &StaticService::Handle_CalculateStandardUserSystemClockDifferenceByUser, "CalculateStandardUserSystemClockDifferenceByUser"},
            {501, &StaticService::Handle_CalculateSpanBetween, "CalculateSpanBetween"},
        };
    // clang-format on

    RegisterHandlers(functions);

    m_set_sys =
        m_system.ServiceManager().GetService<Service::Set::ISystemSettingsServer>("set:sys", true);

    if (m_setup_info.can_write_local_clock && m_setup_info.can_write_user_clock &&
        !m_setup_info.can_write_network_clock && m_setup_info.can_write_timezone_device_location &&
        !m_setup_info.can_write_steady_clock && !m_setup_info.can_write_uninitialized_clock) {
        m_time_m->GetStaticServiceAsAdmin(m_wrapped_service);
    } else if (!m_setup_info.can_write_local_clock && !m_setup_info.can_write_user_clock &&
               !m_setup_info.can_write_network_clock &&
               !m_setup_info.can_write_timezone_device_location &&
               !m_setup_info.can_write_steady_clock &&
               !m_setup_info.can_write_uninitialized_clock) {
        m_time_m->GetStaticServiceAsUser(m_wrapped_service);
    } else if (!m_setup_info.can_write_local_clock && !m_setup_info.can_write_user_clock &&
               !m_setup_info.can_write_network_clock &&
               !m_setup_info.can_write_timezone_device_location &&
               m_setup_info.can_write_steady_clock && !m_setup_info.can_write_uninitialized_clock) {
        m_time_m->GetStaticServiceAsRepair(m_wrapped_service);
    } else {
        UNREACHABLE();
    }

    auto res = m_wrapped_service->GetTimeZoneService(m_time_zone);
    ASSERT(res == ResultSuccess);
}

void StaticService::Handle_GetStandardUserSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<Service::PSC::Time::SystemClock> service{};
    auto res = GetStandardUserSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<Service::PSC::Time::SystemClock>(std::move(service));
}

void StaticService::Handle_GetStandardNetworkSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<Service::PSC::Time::SystemClock> service{};
    auto res = GetStandardNetworkSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<Service::PSC::Time::SystemClock>(std::move(service));
}

void StaticService::Handle_GetStandardSteadyClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<Service::PSC::Time::SteadyClock> service{};
    auto res = GetStandardSteadyClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface(std::move(service));
}

void StaticService::Handle_GetTimeZoneService(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<TimeZoneService> service{};
    auto res = GetTimeZoneService(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface(std::move(service));
}

void StaticService::Handle_GetStandardLocalSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<Service::PSC::Time::SystemClock> service{};
    auto res = GetStandardLocalSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<Service::PSC::Time::SystemClock>(std::move(service));
}

void StaticService::Handle_GetEphemeralNetworkSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<Service::PSC::Time::SystemClock> service{};
    auto res = GetEphemeralNetworkSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<Service::PSC::Time::SystemClock>(std::move(service));
}

void StaticService::Handle_GetSharedMemoryNativeHandle(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KSharedMemory* shared_memory{};
    auto res = GetSharedMemoryNativeHandle(&shared_memory);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(shared_memory);
}

void StaticService::Handle_SetStandardSteadyClockInternalOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto offset_ns{rp.Pop<s64>()};

    auto res = SetStandardSteadyClockInternalOffset(offset_ns);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void StaticService::Handle_GetStandardSteadyClockRtcValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    s64 rtc_value{};
    auto res = GetStandardSteadyClockRtcValue(rtc_value);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(rtc_value);
}

void StaticService::Handle_IsStandardUserSystemClockAutomaticCorrectionEnabled(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    bool is_enabled{};
    auto res = IsStandardUserSystemClockAutomaticCorrectionEnabled(is_enabled);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push<bool>(is_enabled);
}

void StaticService::Handle_SetStandardUserSystemClockAutomaticCorrectionEnabled(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto automatic_correction{rp.Pop<bool>()};

    auto res = SetStandardUserSystemClockAutomaticCorrectionEnabled(automatic_correction);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void StaticService::Handle_GetStandardUserSystemClockInitialYear(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    s32 initial_year{};
    auto res = GetStandardUserSystemClockInitialYear(initial_year);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(initial_year);
}

void StaticService::Handle_IsStandardNetworkSystemClockAccuracySufficient(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    bool is_sufficient{};
    auto res = IsStandardNetworkSystemClockAccuracySufficient(is_sufficient);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push<bool>(is_sufficient);
}

void StaticService::Handle_GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Service::PSC::Time::SteadyClockTimePoint time_point{};
    auto res = GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx,
                            2 + sizeof(Service::PSC::Time::SteadyClockTimePoint) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<Service::PSC::Time::SteadyClockTimePoint>(time_point);
}

void StaticService::Handle_CalculateMonotonicSystemClockBaseTimePoint(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<Service::PSC::Time::SystemClockContext>()};

    s64 time{};
    auto res = CalculateMonotonicSystemClockBaseTimePoint(time, context);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push<s64>(time);
}

void StaticService::Handle_GetClockSnapshot(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto type{rp.PopEnum<Service::PSC::Time::TimeType>()};

    Service::PSC::Time::ClockSnapshot snapshot{};
    auto res = GetClockSnapshot(snapshot, type);

    ctx.WriteBuffer(snapshot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void StaticService::Handle_GetClockSnapshotFromSystemClockContext(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto clock_type{rp.PopEnum<Service::PSC::Time::TimeType>()};
    [[maybe_unused]] auto alignment{rp.Pop<u32>()};
    auto user_context{rp.PopRaw<Service::PSC::Time::SystemClockContext>()};
    auto network_context{rp.PopRaw<Service::PSC::Time::SystemClockContext>()};

    Service::PSC::Time::ClockSnapshot snapshot{};
    auto res =
        GetClockSnapshotFromSystemClockContext(snapshot, user_context, network_context, clock_type);

    ctx.WriteBuffer(snapshot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void StaticService::Handle_CalculateStandardUserSystemClockDifferenceByUser(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Service::PSC::Time::ClockSnapshot a{};
    Service::PSC::Time::ClockSnapshot b{};

    auto a_buffer{ctx.ReadBuffer(0)};
    auto b_buffer{ctx.ReadBuffer(1)};

    std::memcpy(&a, a_buffer.data(), sizeof(Service::PSC::Time::ClockSnapshot));
    std::memcpy(&b, b_buffer.data(), sizeof(Service::PSC::Time::ClockSnapshot));

    s64 difference{};
    auto res = CalculateStandardUserSystemClockDifferenceByUser(difference, a, b);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(difference);
}

void StaticService::Handle_CalculateSpanBetween(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Service::PSC::Time::ClockSnapshot a{};
    Service::PSC::Time::ClockSnapshot b{};

    auto a_buffer{ctx.ReadBuffer(0)};
    auto b_buffer{ctx.ReadBuffer(1)};

    std::memcpy(&a, a_buffer.data(), sizeof(Service::PSC::Time::ClockSnapshot));
    std::memcpy(&b, b_buffer.data(), sizeof(Service::PSC::Time::ClockSnapshot));

    s64 time{};
    auto res = CalculateSpanBetween(time, a, b);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(time);
}

// =============================== Implementations ===========================

Result StaticService::GetStandardUserSystemClock(
    std::shared_ptr<Service::PSC::Time::SystemClock>& out_service) {
    R_RETURN(m_wrapped_service->GetStandardUserSystemClock(out_service));
}

Result StaticService::GetStandardNetworkSystemClock(
    std::shared_ptr<Service::PSC::Time::SystemClock>& out_service) {
    R_RETURN(m_wrapped_service->GetStandardNetworkSystemClock(out_service));
}

Result StaticService::GetStandardSteadyClock(
    std::shared_ptr<Service::PSC::Time::SteadyClock>& out_service) {
    R_RETURN(m_wrapped_service->GetStandardSteadyClock(out_service));
}

Result StaticService::GetTimeZoneService(std::shared_ptr<TimeZoneService>& out_service) {
    out_service = std::make_shared<TimeZoneService>(m_system, m_file_timestamp_worker,
                                                    m_setup_info.can_write_timezone_device_location,
                                                    m_time_zone);
    R_SUCCEED();
}

Result StaticService::GetStandardLocalSystemClock(
    std::shared_ptr<Service::PSC::Time::SystemClock>& out_service) {
    R_RETURN(m_wrapped_service->GetStandardLocalSystemClock(out_service));
}

Result StaticService::GetEphemeralNetworkSystemClock(
    std::shared_ptr<Service::PSC::Time::SystemClock>& out_service) {
    R_RETURN(m_wrapped_service->GetEphemeralNetworkSystemClock(out_service));
}

Result StaticService::GetSharedMemoryNativeHandle(Kernel::KSharedMemory** out_shared_memory) {
    R_RETURN(m_wrapped_service->GetSharedMemoryNativeHandle(out_shared_memory));
}

Result StaticService::SetStandardSteadyClockInternalOffset(s64 offset_ns) {
    R_UNLESS(m_setup_info.can_write_steady_clock, Service::PSC::Time::ResultPermissionDenied);

    R_RETURN(m_set_sys->SetExternalSteadyClockInternalOffset(
        offset_ns /
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()));
}

Result StaticService::GetStandardSteadyClockRtcValue(s64& out_rtc_value) {
    R_RETURN(m_standard_steady_clock_resource.GetRtcTimeInSeconds(out_rtc_value));
}

Result StaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled(
    bool& out_automatic_correction) {
    R_RETURN(m_wrapped_service->IsStandardUserSystemClockAutomaticCorrectionEnabled(
        out_automatic_correction));
}

Result StaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled(
    bool automatic_correction) {
    R_RETURN(m_wrapped_service->SetStandardUserSystemClockAutomaticCorrectionEnabled(
        automatic_correction));
}

Result StaticService::GetStandardUserSystemClockInitialYear(s32& out_year) {
    out_year = GetSettingsItemValue<s32>(m_set_sys, "time", "standard_user_clock_initial_year");
    R_SUCCEED();
}

Result StaticService::IsStandardNetworkSystemClockAccuracySufficient(bool& out_is_sufficient) {
    R_RETURN(m_wrapped_service->IsStandardNetworkSystemClockAccuracySufficient(out_is_sufficient));
}

Result StaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
    Service::PSC::Time::SteadyClockTimePoint& out_time_point) {
    R_RETURN(m_wrapped_service->GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
        out_time_point));
}

Result StaticService::CalculateMonotonicSystemClockBaseTimePoint(
    s64& out_time, Service::PSC::Time::SystemClockContext& context) {
    R_RETURN(m_wrapped_service->CalculateMonotonicSystemClockBaseTimePoint(out_time, context));
}

Result StaticService::GetClockSnapshot(Service::PSC::Time::ClockSnapshot& out_snapshot,
                                       Service::PSC::Time::TimeType type) {
    R_RETURN(m_wrapped_service->GetClockSnapshot(out_snapshot, type));
}

Result StaticService::GetClockSnapshotFromSystemClockContext(
    Service::PSC::Time::ClockSnapshot& out_snapshot,
    Service::PSC::Time::SystemClockContext& user_context,
    Service::PSC::Time::SystemClockContext& network_context, Service::PSC::Time::TimeType type) {
    R_RETURN(m_wrapped_service->GetClockSnapshotFromSystemClockContext(out_snapshot, user_context,
                                                                       network_context, type));
}

Result StaticService::CalculateStandardUserSystemClockDifferenceByUser(
    s64& out_time, Service::PSC::Time::ClockSnapshot& a, Service::PSC::Time::ClockSnapshot& b) {
    R_RETURN(m_wrapped_service->CalculateStandardUserSystemClockDifferenceByUser(out_time, a, b));
}

Result StaticService::CalculateSpanBetween(s64& out_time, Service::PSC::Time::ClockSnapshot& a,
                                           Service::PSC::Time::ClockSnapshot& b) {
    R_RETURN(m_wrapped_service->CalculateSpanBetween(out_time, a, b));
}

} // namespace Service::Glue::Time
