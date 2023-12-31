// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applets/applets.h"
#include "core/hle/service/am/library_applet_accessor.h"
#include "core/hle/service/am/process_winding_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IProcessWindingController::IProcessWindingController(Core::System& system_)
    : ServiceFramework{system_, "IProcessWindingController"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IProcessWindingController::GetLaunchReason, "GetLaunchReason"},
        {11, &IProcessWindingController::OpenCallingLibraryApplet, "OpenCallingLibraryApplet"},
        {21, nullptr, "PushContext"},
        {22, nullptr, "PopContext"},
        {23, nullptr, "CancelWindingReservation"},
        {30, nullptr, "WindAndDoReserved"},
        {40, nullptr, "ReserveToStartAndWaitAndUnwindThis"},
        {41, nullptr, "ReserveToStartAndWait"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IProcessWindingController::~IProcessWindingController() = default;

void IProcessWindingController::GetLaunchReason(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    struct AppletProcessLaunchReason {
        u8 flag;
        INSERT_PADDING_BYTES(3);
    };
    static_assert(sizeof(AppletProcessLaunchReason) == 0x4,
                  "AppletProcessLaunchReason is an invalid size");

    AppletProcessLaunchReason reason{
        .flag = 0,
    };

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw(reason);
}

void IProcessWindingController::OpenCallingLibraryApplet(HLERequestContext& ctx) {
    const auto applet_id = system.GetAppletManager().GetCurrentAppletId();
    const auto applet_mode = Applets::LibraryAppletMode::AllForeground;

    LOG_WARNING(Service_AM, "(STUBBED) called with applet_id={:08X}, applet_mode={:08X}", applet_id,
                applet_mode);

    const auto& applet_manager{system.GetAppletManager()};
    const auto applet = applet_manager.GetApplet(applet_id, applet_mode);

    if (applet == nullptr) {
        LOG_ERROR(Service_AM, "Applet doesn't exist! applet_id={}", applet_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletAccessor>(system, applet);
}

} // namespace Service::AM
