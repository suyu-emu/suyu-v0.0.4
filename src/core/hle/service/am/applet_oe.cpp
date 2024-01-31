// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/am/application_proxy.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

AppletOE::AppletOE(Nvnflinger::Nvnflinger& nvnflinger_, Core::System& system_)
    : ServiceFramework{system_, "appletOE"}, nvnflinger{nvnflinger_} {
    static const FunctionInfo functions[] = {
        {0, &AppletOE::OpenApplicationProxy, "OpenApplicationProxy"},
    };
    RegisterHandlers(functions);
}

AppletOE::~AppletOE() = default;

void AppletOE::OpenApplicationProxy(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    if (const auto applet = GetAppletFromContext(ctx)) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(ResultSuccess);
        rb.PushIpcInterface<IApplicationProxy>(nvnflinger, applet, system);
    } else {
        UNIMPLEMENTED();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
    }
}

std::shared_ptr<Applet> AppletOE::GetAppletFromContext(HLERequestContext& ctx) {
    const auto aruid = ctx.GetPID();
    return system.GetAppletManager().GetByAppletResourceUserId(aruid);
}

} // namespace Service::AM
