// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet_common_functions.h"
#include "core/hle/service/am/application_creator.h"
#include "core/hle/service/am/audio_controller.h"
#include "core/hle/service/am/common_state_getter.h"
#include "core/hle/service/am/debug_functions.h"
#include "core/hle/service/am/display_controller.h"
#include "core/hle/service/am/global_state_controller.h"
#include "core/hle/service/am/home_menu_functions.h"
#include "core/hle/service/am/library_applet_creator.h"
#include "core/hle/service/am/library_applet_self_accessor.h"
#include "core/hle/service/am/process_winding_controller.h"
#include "core/hle/service/am/self_controller.h"
#include "core/hle/service/am/system_applet_proxy.h"
#include "core/hle/service/am/window_controller.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

ISystemAppletProxy::ISystemAppletProxy(Nvnflinger::Nvnflinger& nvnflinger_,
                                       std::shared_ptr<Applet> applet_, Core::System& system_)
    : ServiceFramework{system_, "ISystemAppletProxy"}, nvnflinger{nvnflinger_}, applet{std::move(
                                                                                    applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ISystemAppletProxy::GetCommonStateGetter, "GetCommonStateGetter"},
        {1, &ISystemAppletProxy::GetSelfController, "GetSelfController"},
        {2, &ISystemAppletProxy::GetWindowController, "GetWindowController"},
        {3, &ISystemAppletProxy::GetAudioController, "GetAudioController"},
        {4, &ISystemAppletProxy::GetDisplayController, "GetDisplayController"},
        {10, nullptr, "GetProcessWindingController"},
        {11, &ISystemAppletProxy::GetLibraryAppletCreator, "GetLibraryAppletCreator"},
        {20, &ISystemAppletProxy::GetHomeMenuFunctions, "GetHomeMenuFunctions"},
        {21, &ISystemAppletProxy::GetGlobalStateController, "GetGlobalStateController"},
        {22, &ISystemAppletProxy::GetApplicationCreator, "GetApplicationCreator"},
        {23,  &ISystemAppletProxy::GetAppletCommonFunctions, "GetAppletCommonFunctions"},
        {1000, &ISystemAppletProxy::GetDebugFunctions, "GetDebugFunctions"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

ISystemAppletProxy::~ISystemAppletProxy() = default;

void ISystemAppletProxy::GetCommonStateGetter(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ICommonStateGetter>(system, applet);
}

void ISystemAppletProxy::GetSelfController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ISelfController>(system, applet, nvnflinger);
}

void ISystemAppletProxy::GetWindowController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IWindowController>(system, applet);
}

void ISystemAppletProxy::GetAudioController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAudioController>(system);
}

void ISystemAppletProxy::GetDisplayController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDisplayController>(system, applet);
}

void ISystemAppletProxy::GetLibraryAppletCreator(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletCreator>(system, applet);
}

void ISystemAppletProxy::GetHomeMenuFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IHomeMenuFunctions>(system);
}

void ISystemAppletProxy::GetGlobalStateController(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IGlobalStateController>(system);
}

void ISystemAppletProxy::GetApplicationCreator(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationCreator>(system);
}

void ISystemAppletProxy::GetAppletCommonFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IAppletCommonFunctions>(system, applet);
}

void ISystemAppletProxy::GetDebugFunctions(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IDebugFunctions>(system);
}

} // namespace Service::AM
