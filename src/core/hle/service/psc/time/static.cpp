// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/service/psc/time/clocks/ephemeral_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_local_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_network_system_clock_core.h"
#include "core/hle/service/psc/time/clocks/standard_user_system_clock_core.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/psc/time/shared_memory.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/steady_clock.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/psc/time/time_zone.h"
#include "core/hle/service/psc/time/time_zone_service.h"

namespace Service::PSC::Time {
namespace {
constexpr Result GetTimeFromTimePointAndContext(s64* out_time, SteadyClockTimePoint& time_point,
                                                SystemClockContext& context) {
    R_UNLESS(out_time != nullptr, ResultInvalidArgument);
    R_UNLESS(time_point.IdMatches(context.steady_time_point), ResultClockMismatch);

    *out_time = context.offset + time_point.time_point;
    R_SUCCEED();
}
} // namespace

StaticService::StaticService(Core::System& system_, StaticServiceSetupInfo setup_info,
                             std::shared_ptr<TimeManager> time, const char* name)
    : ServiceFramework{system_, name}, m_system{system}, m_setup_info{setup_info}, m_time{time},
      m_local_system_clock{m_time->m_standard_local_system_clock},
      m_user_system_clock{m_time->m_standard_user_system_clock},
      m_network_system_clock{m_time->m_standard_network_system_clock},
      m_time_zone{m_time->m_time_zone},
      m_ephemeral_network_clock{m_time->m_ephemeral_network_clock}, m_shared_memory{
                                                                        m_time->m_shared_memory} {
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
}

Result StaticService::GetClockSnapshotImpl(ClockSnapshot& out_snapshot,
                                           SystemClockContext& user_context,
                                           SystemClockContext& network_context, TimeType type) {
    out_snapshot.user_context = user_context;
    out_snapshot.network_context = network_context;

    R_TRY(
        m_time->m_standard_steady_clock.GetCurrentTimePoint(out_snapshot.steady_clock_time_point));

    out_snapshot.is_automatic_correction_enabled = m_user_system_clock.GetAutomaticCorrection();

    R_TRY(m_time_zone.GetLocationName(out_snapshot.location_name));

    R_TRY(GetTimeFromTimePointAndContext(
        &out_snapshot.user_time, out_snapshot.steady_clock_time_point, out_snapshot.user_context));

    R_TRY(m_time_zone.ToCalendarTimeWithMyRule(out_snapshot.user_calendar_time,
                                               out_snapshot.user_calendar_additional_time,
                                               out_snapshot.user_time));

    if (GetTimeFromTimePointAndContext(&out_snapshot.network_time,
                                       out_snapshot.steady_clock_time_point,
                                       out_snapshot.network_context) != ResultSuccess) {
        out_snapshot.network_time = 0;
    }

    R_TRY(m_time_zone.ToCalendarTimeWithMyRule(out_snapshot.network_calendar_time,
                                               out_snapshot.network_calendar_additional_time,
                                               out_snapshot.network_time));
    out_snapshot.type = type;
    out_snapshot.unk_CE = 0;
    R_SUCCEED();
}

void StaticService::Handle_GetStandardUserSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<SystemClock> service{};
    auto res = GetStandardUserSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<SystemClock>(std::move(service));
}

void StaticService::Handle_GetStandardNetworkSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<SystemClock> service{};
    auto res = GetStandardNetworkSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<SystemClock>(std::move(service));
}

void StaticService::Handle_GetStandardSteadyClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<SteadyClock> service{};
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

    std::shared_ptr<SystemClock> service{};
    auto res = GetStandardLocalSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<SystemClock>(std::move(service));
}

void StaticService::Handle_GetEphemeralNetworkSystemClock(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    std::shared_ptr<SystemClock> service{};
    auto res = GetEphemeralNetworkSystemClock(service);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(res);
    rb.PushIpcInterface<SystemClock>(std::move(service));
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

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(m_setup_info.can_write_steady_clock ? ResultNotImplemented : ResultPermissionDenied);
}

void StaticService::Handle_GetStandardSteadyClockRtcValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNotImplemented);
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

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultNotImplemented);
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

    SteadyClockTimePoint time_point{};
    auto res = GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(time_point);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(SteadyClockTimePoint) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<SteadyClockTimePoint>(time_point);
}

void StaticService::Handle_CalculateMonotonicSystemClockBaseTimePoint(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<SystemClockContext>()};

    s64 time{};
    auto res = CalculateMonotonicSystemClockBaseTimePoint(time, context);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push<s64>(time);
}

void StaticService::Handle_GetClockSnapshot(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto type{rp.PopEnum<TimeType>()};

    ClockSnapshot snapshot{};
    auto res = GetClockSnapshot(snapshot, type);

    ctx.WriteBuffer(snapshot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void StaticService::Handle_GetClockSnapshotFromSystemClockContext(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto clock_type{rp.PopEnum<TimeType>()};
    [[maybe_unused]] auto alignment{rp.Pop<u32>()};
    auto user_context{rp.PopRaw<SystemClockContext>()};
    auto network_context{rp.PopRaw<SystemClockContext>()};

    ClockSnapshot snapshot{};
    auto res =
        GetClockSnapshotFromSystemClockContext(snapshot, user_context, network_context, clock_type);

    ctx.WriteBuffer(snapshot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void StaticService::Handle_CalculateStandardUserSystemClockDifferenceByUser(
    HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    ClockSnapshot a{};
    ClockSnapshot b{};

    auto a_buffer{ctx.ReadBuffer(0)};
    auto b_buffer{ctx.ReadBuffer(1)};

    std::memcpy(&a, a_buffer.data(), sizeof(ClockSnapshot));
    std::memcpy(&b, b_buffer.data(), sizeof(ClockSnapshot));

    s64 difference{};
    auto res = CalculateStandardUserSystemClockDifferenceByUser(difference, a, b);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(difference);
}

void StaticService::Handle_CalculateSpanBetween(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    ClockSnapshot a{};
    ClockSnapshot b{};

    auto a_buffer{ctx.ReadBuffer(0)};
    auto b_buffer{ctx.ReadBuffer(1)};

    std::memcpy(&a, a_buffer.data(), sizeof(ClockSnapshot));
    std::memcpy(&b, b_buffer.data(), sizeof(ClockSnapshot));

    s64 time{};
    auto res = CalculateSpanBetween(time, a, b);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(time);
}

// =============================== Implementations ===========================

Result StaticService::GetStandardUserSystemClock(std::shared_ptr<SystemClock>& out_service) {
    out_service = std::make_shared<SystemClock>(m_system, m_user_system_clock,
                                                m_setup_info.can_write_user_clock,
                                                m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetStandardNetworkSystemClock(std::shared_ptr<SystemClock>& out_service) {
    out_service = std::make_shared<SystemClock>(m_system, m_network_system_clock,
                                                m_setup_info.can_write_network_clock,
                                                m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetStandardSteadyClock(std::shared_ptr<SteadyClock>& out_service) {
    out_service =
        std::make_shared<SteadyClock>(m_system, m_time, m_setup_info.can_write_steady_clock,
                                      m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetTimeZoneService(std::shared_ptr<TimeZoneService>& out_service) {
    out_service =
        std::make_shared<TimeZoneService>(m_system, m_time->m_standard_steady_clock, m_time_zone,
                                          m_setup_info.can_write_timezone_device_location);
    R_SUCCEED();
}

Result StaticService::GetStandardLocalSystemClock(std::shared_ptr<SystemClock>& out_service) {
    out_service = std::make_shared<SystemClock>(m_system, m_local_system_clock,
                                                m_setup_info.can_write_local_clock,
                                                m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetEphemeralNetworkSystemClock(std::shared_ptr<SystemClock>& out_service) {
    out_service = std::make_shared<SystemClock>(m_system, m_ephemeral_network_clock,
                                                m_setup_info.can_write_network_clock,
                                                m_setup_info.can_write_uninitialized_clock);
    R_SUCCEED();
}

Result StaticService::GetSharedMemoryNativeHandle(Kernel::KSharedMemory** out_shared_memory) {
    *out_shared_memory = &m_shared_memory.GetKSharedMemory();
    R_SUCCEED();
}

Result StaticService::IsStandardUserSystemClockAutomaticCorrectionEnabled(bool& out_is_enabled) {
    R_UNLESS(m_user_system_clock.IsInitialized(), ResultClockUninitialized);

    out_is_enabled = m_user_system_clock.GetAutomaticCorrection();
    R_SUCCEED();
}

Result StaticService::SetStandardUserSystemClockAutomaticCorrectionEnabled(
    bool automatic_correction) {
    R_UNLESS(m_user_system_clock.IsInitialized() && m_time->m_standard_steady_clock.IsInitialized(),
             ResultClockUninitialized);
    R_UNLESS(m_setup_info.can_write_user_clock, ResultPermissionDenied);

    R_TRY(m_user_system_clock.SetAutomaticCorrection(automatic_correction));

    m_shared_memory.SetAutomaticCorrection(automatic_correction);

    SteadyClockTimePoint time_point{};
    R_TRY(m_time->m_standard_steady_clock.GetCurrentTimePoint(time_point));

    m_user_system_clock.SetTimePointAndSignal(time_point);
    m_user_system_clock.GetEvent().Signal();
    R_SUCCEED();
}

Result StaticService::IsStandardNetworkSystemClockAccuracySufficient(bool& out_is_sufficient) {
    out_is_sufficient = m_network_system_clock.IsAccuracySufficient();
    R_SUCCEED();
}

Result StaticService::GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(
    SteadyClockTimePoint& out_time_point) {
    R_UNLESS(m_user_system_clock.IsInitialized(), ResultClockUninitialized);

    m_user_system_clock.GetTimePoint(out_time_point);

    R_SUCCEED();
}

Result StaticService::CalculateMonotonicSystemClockBaseTimePoint(s64& out_time,
                                                                 SystemClockContext& context) {
    R_UNLESS(m_time->m_standard_steady_clock.IsInitialized(), ResultClockUninitialized);

    SteadyClockTimePoint time_point{};
    R_TRY(m_time->m_standard_steady_clock.GetCurrentTimePoint(time_point));

    R_UNLESS(time_point.IdMatches(context.steady_time_point), ResultClockMismatch);

    auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
    auto ticks{m_system.CoreTiming().GetClockTicks()};
    auto current_time{ConvertToTimeSpan(ticks).count()};
    out_time = ((context.offset + time_point.time_point) - (current_time / one_second_ns));
    R_SUCCEED();
}

Result StaticService::GetClockSnapshot(ClockSnapshot& out_snapshot, TimeType type) {
    SystemClockContext user_context{};
    R_TRY(m_user_system_clock.GetContext(user_context));

    SystemClockContext network_context{};
    R_TRY(m_network_system_clock.GetContext(network_context));

    R_RETURN(GetClockSnapshotImpl(out_snapshot, user_context, network_context, type));
}

Result StaticService::GetClockSnapshotFromSystemClockContext(ClockSnapshot& out_snapshot,
                                                             SystemClockContext& user_context,
                                                             SystemClockContext& network_context,
                                                             TimeType type) {
    R_RETURN(GetClockSnapshotImpl(out_snapshot, user_context, network_context, type));
}

Result StaticService::CalculateStandardUserSystemClockDifferenceByUser(s64& out_time,
                                                                       ClockSnapshot& a,
                                                                       ClockSnapshot& b) {
    auto diff_s =
        std::chrono::seconds(b.user_context.offset) - std::chrono::seconds(a.user_context.offset);

    if (a.user_context == b.user_context ||
        !a.user_context.steady_time_point.IdMatches(b.user_context.steady_time_point)) {
        out_time = 0;
        R_SUCCEED();
    }

    if (!a.is_automatic_correction_enabled || !b.is_automatic_correction_enabled) {
        out_time = std::chrono::duration_cast<std::chrono::nanoseconds>(diff_s).count();
        R_SUCCEED();
    }

    if (a.network_context.steady_time_point.IdMatches(a.steady_clock_time_point) ||
        b.network_context.steady_time_point.IdMatches(b.steady_clock_time_point)) {
        out_time = 0;
        R_SUCCEED();
    }

    out_time = std::chrono::duration_cast<std::chrono::nanoseconds>(diff_s).count();
    R_SUCCEED();
}

Result StaticService::CalculateSpanBetween(s64& out_time, ClockSnapshot& a, ClockSnapshot& b) {
    s64 time_s{};
    auto res =
        GetSpanBetweenTimePoints(&time_s, a.steady_clock_time_point, b.steady_clock_time_point);

    if (res != ResultSuccess) {
        R_UNLESS(a.network_time != 0 && b.network_time != 0, ResultTimeNotFound);
        time_s = b.network_time - a.network_time;
    }

    out_time =
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(time_s)).count();
    R_SUCCEED();
}

} // namespace Service::PSC::Time
