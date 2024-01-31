// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet_ae.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/library_applet_proxy.h"
#include "core/hle/service/am/system_applet_proxy.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

AppletAE::AppletAE(Nvnflinger::Nvnflinger& nvnflinger_, Core::System& system_)
    : ServiceFramework{system_, "appletAE"}, nvnflinger{nvnflinger_} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {100, &AppletAE::OpenSystemAppletProxy, "OpenSystemAppletProxy"},
        {200, &AppletAE::OpenLibraryAppletProxyOld, "OpenLibraryAppletProxyOld"},
        {201, &AppletAE::OpenLibraryAppletProxy, "OpenLibraryAppletProxy"},
        {300, nullptr, "OpenOverlayAppletProxy"},
        {350, nullptr, "OpenSystemApplicationProxy"},
        {400, nullptr, "CreateSelfLibraryAppletCreatorForDevelop"},
        {410, nullptr, "GetSystemAppletControllerForDebug"},
        {1000, nullptr, "GetDebugFunctions"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

AppletAE::~AppletAE() = default;

void AppletAE::OpenSystemAppletProxy(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    if (const auto applet = GetAppletFromContext(ctx)) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ISystemAppletProxy>(nvnflinger, applet, system);
    } else {
        UNIMPLEMENTED();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
    }
}

void AppletAE::OpenLibraryAppletProxy(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    if (const auto applet = GetAppletFromContext(ctx)) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<ILibraryAppletProxy>(nvnflinger, applet, system);
    } else {
        UNIMPLEMENTED();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
    }
}

void AppletAE::OpenLibraryAppletProxyOld(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    return OpenLibraryAppletProxy(ctx);
}

std::shared_ptr<Applet> AppletAE::GetAppletFromContext(HLERequestContext& ctx) {
    const auto aruid = ctx.GetPID();
    return system.GetAppletManager().GetByAppletResourceUserId(aruid);
}

} // namespace Service::AM
