// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am.h"
#include "core/hle/service/am/applet_oe.h"
#include "core/hle/service/am/application_proxy.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

void AppletOE::OpenApplicationProxy(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationProxy>(nvnflinger, msg_queue, system);
}

AppletOE::AppletOE(Nvnflinger::Nvnflinger& nvnflinger_,
                   std::shared_ptr<AppletMessageQueue> msg_queue_, Core::System& system_)
    : ServiceFramework{system_, "appletOE"}, nvnflinger{nvnflinger_},
      msg_queue{std::move(msg_queue_)} {
    static const FunctionInfo functions[] = {
        {0, &AppletOE::OpenApplicationProxy, "OpenApplicationProxy"},
    };
    RegisterHandlers(functions);
}

AppletOE::~AppletOE() = default;

const std::shared_ptr<AppletMessageQueue>& AppletOE::GetMessageQueue() const {
    return msg_queue;
}

} // namespace Service::AM
