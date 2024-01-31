// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet_common_functions.h"
#include "core/hle/service/am/audio_controller.h"
#include "core/hle/service/am/common_state_getter.h"
#include "core/hle/service/am/debug_functions.h"
#include "core/hle/service/am/display_controller.h"
#include "core/hle/service/am/global_state_controller.h"
#include "core/hle/service/am/home_menu_functions.h"
#include "core/hle/service/am/library_applet_creator.h"
#include "core/hle/service/am/library_applet_proxy.h"
#include "core/hle/service/am/library_applet_self_accessor.h"
#include "core/hle/service/am/process_winding_controller.h"
#include "core/hle/service/am/self_controller.h"
#include "core/hle/service/am/window_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

ILibraryAppletProxy::ILibraryAppletProxy(Nvnflinger::Nvnflinger& nvnflinger_,
                                         std::shared_ptr<Applet> applet_, Core::System& system_)
    : ServiceFramework{system_, "ILibraryAppletProxy"}, nvnflinger{nvnflinger_}, applet{std::move(
                                                                                     applet_)} {
    // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ILibraryAppletProxy::GetCommonStateGetter, "GetCommonStateGetter"},
            {1, &ILibraryAppletProxy::GetSelfController, "GetSelfController"},
            {2, &ILibraryAppletProxy::GetWindowController, "GetWindowController"},
            {3, &ILibraryAppletProxy::GetAudioController, "GetAudioController"},
            {4, &ILibraryAppletProxy::GetDisplayController, "GetDisplayController"},
            {10, &ILibraryAppletProxy::GetProcessWindingController, "GetProcessWindingController"},
            {11, &ILibraryAppletProxy::GetLibraryAppletCreator, "GetLibraryAppletCreator"},
            {20, &ILibraryAppletProxy::OpenLibraryAppletSelfAccessor, "OpenLibraryAppletSelfAccessor"},
            {21, &ILibraryAppletProxy::GetAppletCommonFunctions, "GetAppletCommonFunctions"},
            {22, &ILibraryAppletProxy::GetHomeMenuFunctions, "GetHomeMenuFunctions"},
            {23, &ILibraryAppletProxy::GetGlobalStateController, "GetGlobalStateController"},
            {1000, &ILibraryAppletProxy::GetDebugFunctions, "GetDebugFunctions"},
        };
    // clang-format on

    RegisterHandlers(functions);
}

ILibraryAppletProxy::~ILibraryAppletProxy() = default;

void ILibraryAppletProxy::GetCommonStateGetter(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ICommonStateGetter>(system, applet);
}

void ILibraryAppletProxy::GetSelfController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISelfController>(system, applet, nvnflinger);
}

void ILibraryAppletProxy::GetWindowController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IWindowController>(system, applet);
}

void ILibraryAppletProxy::GetAudioController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAudioController>(system);
}

void ILibraryAppletProxy::GetDisplayController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDisplayController>(system, applet);
}

void ILibraryAppletProxy::GetProcessWindingController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IProcessWindingController>(system, applet);
}

void ILibraryAppletProxy::GetLibraryAppletCreator(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletCreator>(system, applet);
}

void ILibraryAppletProxy::OpenLibraryAppletSelfAccessor(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletSelfAccessor>(system, applet);
}

void ILibraryAppletProxy::GetAppletCommonFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAppletCommonFunctions>(system, applet);
}

void ILibraryAppletProxy::GetHomeMenuFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IHomeMenuFunctions>(system);
}

void ILibraryAppletProxy::GetGlobalStateController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IGlobalStateController>(system);
}

void ILibraryAppletProxy::GetDebugFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDebugFunctions>(system);
}

} // namespace Service::AM
