// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/psc/time/power_state_service.h"

namespace Service::PSC::Time {

IPowerStateRequestHandler::IPowerStateRequestHandler(
    Core::System& system_, PowerStateRequestManager& power_state_request_manager)
    : ServiceFramework{system_, "time:p"}, m_system{system}, m_power_state_request_manager{
                                                                 power_state_request_manager} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IPowerStateRequestHandler::GetPowerStateRequestEventReadableHandle, "GetPowerStateRequestEventReadableHandle"},
            {1, &IPowerStateRequestHandler::GetAndClearPowerStateRequest, "GetAndClearPowerStateRequest"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

void IPowerStateRequestHandler::GetPowerStateRequestEventReadableHandle(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(m_power_state_request_manager.GetReadableEvent());
}

void IPowerStateRequestHandler::GetAndClearPowerStateRequest(HLERequestContext& ctx) {
    LOG_DEBUG(Service_Time, "called.");

    u32 priority{};
    auto cleared = m_power_state_request_manager.GetAndClearPowerStateRequest(priority);

    if (cleared) {
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(ResultSuccess);
        rb.Push(priority);
        rb.Push(cleared);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(cleared);
}

} // namespace Service::PSC::Time
