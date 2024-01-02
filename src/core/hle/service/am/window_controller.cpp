// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/window_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IWindowController::IWindowController(Core::System& system_, std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "IWindowController"}, applet{std::move(applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "CreateWindow"},
        {1, &IWindowController::GetAppletResourceUserId, "GetAppletResourceUserId"},
        {2, &IWindowController::GetAppletResourceUserIdOfCallerApplet, "GetAppletResourceUserIdOfCallerApplet"},
        {10, &IWindowController::AcquireForegroundRights, "AcquireForegroundRights"},
        {11, nullptr, "ReleaseForegroundRights"},
        {12, nullptr, "RejectToChangeIntoBackground"},
        {20, nullptr, "SetAppletWindowVisibility"},
        {21, nullptr, "SetAppletGpuTimeSlice"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IWindowController::~IWindowController() = default;

void IWindowController::GetAppletResourceUserId(HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(applet->aruid);
}

void IWindowController::GetAppletResourceUserIdOfCallerApplet(HLERequestContext& ctx) {
    u64 aruid = 0;
    if (auto caller = applet->caller_applet.lock(); caller) {
        aruid = caller->aruid;
    }

    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(aruid);
}

void IWindowController::AcquireForegroundRights(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

} // namespace Service::AM
