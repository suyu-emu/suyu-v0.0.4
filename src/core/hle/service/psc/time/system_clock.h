// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class SystemClock final : public ServiceFramework<SystemClock> {
public:
    explicit SystemClock(Core::System& system, SystemClockCore& system_clock_core,
                         bool can_write_clock, bool can_write_uninitialized_clock);

    ~SystemClock() override = default;

    Result GetCurrentTime(s64& out_time);
    Result SetCurrentTime(s64 time);
    Result GetSystemClockContext(SystemClockContext& out_context);
    Result SetSystemClockContext(SystemClockContext& context);
    Result GetOperationEventReadableHandle(Kernel::KEvent** out_event);

private:
    void Handle_GetCurrentTime(HLERequestContext& ctx);
    void Handle_SetCurrentTime(HLERequestContext& ctx);
    void Handle_GetSystemClockContext(HLERequestContext& ctx);
    void Handle_SetSystemClockContext(HLERequestContext& ctx);
    void Handle_GetOperationEventReadableHandle(HLERequestContext& ctx);

    Core::System& m_system;

    SystemClockCore& m_clock_core;
    bool m_can_write_clock;
    bool m_can_write_uninitialized_clock;
    std::unique_ptr<OperationEvent> m_operation_event{};
};

} // namespace Service::PSC::Time
