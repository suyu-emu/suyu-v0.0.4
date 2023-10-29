// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/psc/time/system_clock.h"

namespace Service::PSC::Time {

SystemClock::SystemClock(Core::System& system_, SystemClockCore& clock_core, bool can_write_clock,
                         bool can_write_uninitialized_clock)
    : ServiceFramework{system_, "ISystemClock"}, m_system{system}, m_clock_core{clock_core},
      m_can_write_clock{can_write_clock}, m_can_write_uninitialized_clock{
                                              can_write_uninitialized_clock} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &SystemClock::Handle_GetCurrentTime, "GetCurrentTime"},
        {1, &SystemClock::Handle_SetCurrentTime, "SetCurrentTime"},
        {2, &SystemClock::Handle_GetSystemClockContext, "GetSystemClockContext"},
        {3, &SystemClock::Handle_SetSystemClockContext, "SetSystemClockContext"},
        {4, &SystemClock::Handle_GetOperationEventReadableHandle, "GetOperationEventReadableHandle"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

void SystemClock::Handle_GetCurrentTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    s64 time{};
    auto res = GetCurrentTime(time);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(res);
    rb.Push<s64>(time);
}

void SystemClock::Handle_SetCurrentTime(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto time{rp.Pop<s64>()};

    auto res = SetCurrentTime(time);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SystemClock::Handle_GetSystemClockContext(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    SystemClockContext context{};
    auto res = GetSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx, 2 + sizeof(SystemClockContext) / sizeof(u32)};
    rb.Push(res);
    rb.PushRaw<SystemClockContext>(context);
}

void SystemClock::Handle_SetSystemClockContext(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::RequestParser rp{ctx};
    auto context{rp.PopRaw<SystemClockContext>()};

    auto res = SetSystemClockContext(context);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(res);
}

void SystemClock::Handle_GetOperationEventReadableHandle(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    Kernel::KEvent* event{};
    auto res = GetOperationEventReadableHandle(&event);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(res);
    rb.PushCopyObjects(event->GetReadableEvent());
}

// =============================== Implementations ===========================

Result SystemClock::GetCurrentTime(s64& out_time) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetCurrentTime(&out_time));
}

Result SystemClock::SetCurrentTime(s64 time) {
    R_UNLESS(m_can_write_clock, ResultPermissionDenied);
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.SetCurrentTime(time));
}

Result SystemClock::GetSystemClockContext(SystemClockContext& out_context) {
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.GetContext(out_context));
}

Result SystemClock::SetSystemClockContext(SystemClockContext& context) {
    R_UNLESS(m_can_write_clock, ResultPermissionDenied);
    R_UNLESS(m_can_write_uninitialized_clock || m_clock_core.IsInitialized(),
             ResultClockUninitialized);

    R_RETURN(m_clock_core.SetContextAndWrite(context));
}

Result SystemClock::GetOperationEventReadableHandle(Kernel::KEvent** out_event) {
    if (!m_operation_event) {
        m_operation_event = std::make_unique<OperationEvent>(m_system);
        R_UNLESS(m_operation_event != nullptr, ResultFailed);

        m_clock_core.LinkOperationEvent(*m_operation_event);
    }

    *out_event = m_operation_event->m_event;
    R_SUCCEED();
}

} // namespace Service::PSC::Time
