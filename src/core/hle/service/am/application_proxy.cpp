// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet_common_functions.h"
#include "core/hle/service/am/application_functions.h"
#include "core/hle/service/am/application_proxy.h"
#include "core/hle/service/am/audio_controller.h"
#include "core/hle/service/am/common_state_getter.h"
#include "core/hle/service/am/debug_functions.h"
#include "core/hle/service/am/display_controller.h"
#include "core/hle/service/am/library_applet_creator.h"
#include "core/hle/service/am/library_applet_self_accessor.h"
#include "core/hle/service/am/process_winding_controller.h"
#include "core/hle/service/am/self_controller.h"
#include "core/hle/service/am/window_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IApplicationProxy::IApplicationProxy(Nvnflinger::Nvnflinger& nvnflinger_,
                                     std::shared_ptr<Applet> applet_, Core::System& system_)
    : ServiceFramework{system_, "IApplicationProxy"}, nvnflinger{nvnflinger_}, applet{std::move(
                                                                                   applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IApplicationProxy::GetCommonStateGetter, "GetCommonStateGetter"},
        {1, &IApplicationProxy::GetSelfController, "GetSelfController"},
        {2, &IApplicationProxy::GetWindowController, "GetWindowController"},
        {3, &IApplicationProxy::GetAudioController, "GetAudioController"},
        {4, &IApplicationProxy::GetDisplayController, "GetDisplayController"},
        {10, &IApplicationProxy::GetProcessWindingController, "GetProcessWindingController"},
        {11, &IApplicationProxy::GetLibraryAppletCreator, "GetLibraryAppletCreator"},
        {20, &IApplicationProxy::GetApplicationFunctions, "GetApplicationFunctions"},
        {1000, &IApplicationProxy::GetDebugFunctions, "GetDebugFunctions"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IApplicationProxy::~IApplicationProxy() = default;

void IApplicationProxy::GetAudioController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAudioController>(system);
}

void IApplicationProxy::GetDisplayController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDisplayController>(system, applet);
}

void IApplicationProxy::GetProcessWindingController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IProcessWindingController>(system, applet);
}

void IApplicationProxy::GetDebugFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDebugFunctions>(system);
}

void IApplicationProxy::GetWindowController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IWindowController>(system, applet);
}

void IApplicationProxy::GetSelfController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISelfController>(system, applet, nvnflinger);
}

void IApplicationProxy::GetCommonStateGetter(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ICommonStateGetter>(system, applet);
}

void IApplicationProxy::GetLibraryAppletCreator(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletCreator>(system, applet);
}

void IApplicationProxy::GetApplicationFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationFunctions>(system, applet);
}

} // namespace Service::AM
