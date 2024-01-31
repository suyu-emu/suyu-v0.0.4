// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/home_menu_functions.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IHomeMenuFunctions::IHomeMenuFunctions(Core::System& system_)
    : ServiceFramework{system_, "IHomeMenuFunctions"}, service_context{system,
                                                                       "IHomeMenuFunctions"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {10, &IHomeMenuFunctions::RequestToGetForeground, "RequestToGetForeground"},
        {11, nullptr, "LockForeground"},
        {12, nullptr, "UnlockForeground"},
        {20, nullptr, "PopFromGeneralChannel"},
        {21, &IHomeMenuFunctions::GetPopFromGeneralChannelEvent, "GetPopFromGeneralChannelEvent"},
        {30, nullptr, "GetHomeButtonWriterLockAccessor"},
        {31, nullptr, "GetWriterLockAccessorEx"},
        {40, nullptr, "IsSleepEnabled"},
        {41, nullptr, "IsRebootEnabled"},
        {50, nullptr, "LaunchSystemApplet"},
        {51, nullptr, "LaunchStarter"},
        {100, nullptr, "PopRequestLaunchApplicationForDebug"},
        {110, nullptr, "IsForceTerminateApplicationDisabledForDebug"},
        {200, nullptr, "LaunchDevMenu"},
        {1000, nullptr, "SetLastApplicationExitReason"},
    };
    // clang-format on

    RegisterHandlers(functions);

    pop_from_general_channel_event =
        service_context.CreateEvent("IHomeMenuFunctions:PopFromGeneralChannelEvent");
}

IHomeMenuFunctions::~IHomeMenuFunctions() {
    service_context.CloseEvent(pop_from_general_channel_event);
}

void IHomeMenuFunctions::RequestToGetForeground(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHomeMenuFunctions::GetPopFromGeneralChannelEvent(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(pop_from_general_channel_event->GetReadableEvent());
}

} // namespace Service::AM
