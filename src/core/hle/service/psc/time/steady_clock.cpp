// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/psc/time/steady_clock.h"

namespace Service::PSC::Time {

SteadyClock::SteadyClock(Core::System& system_, std::shared_ptr<TimeManager> manager,
                         bool can_write_steady_clock, bool can_write_uninitialized_clock)
    : ServiceFramework{system_, "ISteadyClock"}, m_system{system},
      m_clock_core{manager->m_standard_steady_clock},
      m_can_write_steady_clock{can_write_steady_clock}, m_can_write_uninitialized_clock{
                                                            can_write_uninitialized_clock} {
    // clang-format off
         static const FunctionInfo functions[] = {
        {0, &SteadyClock::Handle_GetCurrentTimePoint, "GetCurrentTimePoint"},
        {2, &SteadyClock::Handle_GetTestOffset, "GetTestOffset"},
        {3, &SteadyClock::Handle_SetTestOffset, "SetTestOffset"},
        {100, &SteadyClock::Handle_GetRtcValue, "GetRtcValue"},
        {101, &SteadyClock::Handle_IsRtcResetDetected, "IsRtcResetDetected"},
        {102, &SteadyClock::Handle_GetSetupResultValue, "GetSetupResultValue"},
        {200, &SteadyClock::Handle_GetInternalOffset, "GetInternalOffset"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

void SteadyClock::Handle_GetCurrentTimePoint(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    SteadyClockTimePoint time_point{};
    auto res = GetCurrentTimePoint(time_point);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(SteadyClockTimePoint) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<SteadyClockTimePoint>(time_point);
}

void SteadyClock::Handle_GetTestOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    s64 test_offset{};
    auto res = GetTestOffset(test_offset);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(test_offset);
}

void SteadyClock::Handle_SetTestOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto test_offset{rp.Pop<s64>()};

    auto res = SetTestOffset(test_offset);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SteadyClock::Handle_GetRtcValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    s64 rtc_value{};
    auto res = GetRtcValue(rtc_value);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(rtc_value);
}

void SteadyClock::Handle_IsRtcResetDetected(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    bool reset_detected{false};
    auto res = IsRtcResetDetected(reset_detected);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(reset_detected);
}

void SteadyClock::Handle_GetSetupResultValue(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Result result_value{ResultSuccess};
    auto res = GetSetupResultValue(result_value);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(res);
    rb.Push(result_value);
}

void SteadyClock::Handle_GetInternalOffset(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    s64 internal_offset{};
    auto res = GetInternalOffset(internal_offset);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push(internal_offset);
}

// =============================== Implementations ===========================

Result SteadyClock::GetCurrentTimePoint(SteadyClockTimePoint& out_time_point) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetCurrentTimePoint(out_time_point));
}

Result SteadyClock::GetTestOffset(s64& out_test_offset) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    out_test_offset = m_clock_core.GetTestOffset();
    R_SUCCEED();
}

Result SteadyClock::SetTestOffset(s64 test_offset) {
    R_UNLESS(m_can_write_steady_clock, ResultPermissionDenied);
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    m_clock_core.SetTestOffset(test_offset);
    R_SUCCEED();
}

Result SteadyClock::GetRtcValue(s64& out_rtc_value) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetRtcValue(out_rtc_value));
}

Result SteadyClock::IsRtcResetDetected(bool& out_is_detected) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    out_is_detected = m_clock_core.IsResetDetected();
    R_SUCCEED();
}

Result SteadyClock::GetSetupResultValue(Result& out_result) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    out_result = m_clock_core.GetSetupResultValue();
    R_SUCCEED();
}

Result SteadyClock::GetInternalOffset(s64& out_internal_offset) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    out_internal_offset = m_clock_core.GetInternalOffset();
    R_SUCCEED();
}

} // namespace Service::PSC::Time
