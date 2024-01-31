// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet_data_broker.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/library_applet_accessor.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

ILibraryAppletAccessor::ILibraryAppletAccessor(Core::System& system_,
                                               std::shared_ptr<AppletDataBroker> broker_,
                                               std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "ILibraryAppletAccessor"}, broker{std::move(broker_)},
      applet{std::move(applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ILibraryAppletAccessor::GetAppletStateChangedEvent, "GetAppletStateChangedEvent"},
        {1, &ILibraryAppletAccessor::IsCompleted, "IsCompleted"},
        {10, &ILibraryAppletAccessor::Start, "Start"},
        {20, &ILibraryAppletAccessor::RequestExit, "RequestExit"},
        {25, nullptr, "Terminate"},
        {30, &ILibraryAppletAccessor::GetResult, "GetResult"},
        {50, nullptr, "SetOutOfFocusApplicationSuspendingEnabled"},
        {60, &ILibraryAppletAccessor::PresetLibraryAppletGpuTimeSliceZero, "PresetLibraryAppletGpuTimeSliceZero"},
        {100, &ILibraryAppletAccessor::PushInData, "PushInData"},
        {101, &ILibraryAppletAccessor::PopOutData, "PopOutData"},
        {102, nullptr, "PushExtraStorage"},
        {103, &ILibraryAppletAccessor::PushInteractiveInData, "PushInteractiveInData"},
        {104, &ILibraryAppletAccessor::PopInteractiveOutData, "PopInteractiveOutData"},
        {105, &ILibraryAppletAccessor::GetPopOutDataEvent, "GetPopOutDataEvent"},
        {106, &ILibraryAppletAccessor::GetPopInteractiveOutDataEvent, "GetPopInteractiveOutDataEvent"},
        {110, nullptr, "NeedsToExitProcess"},
        {120, nullptr, "GetLibraryAppletInfo"},
        {150, nullptr, "RequestForAppletToGetForeground"},
        {160, &ILibraryAppletAccessor::GetIndirectLayerConsumerHandle, "GetIndirectLayerConsumerHandle"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

ILibraryAppletAccessor::~ILibraryAppletAccessor() = default;

void ILibraryAppletAccessor::GetAppletStateChangedEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetStateChangedEvent().GetHandle());
}

void ILibraryAppletAccessor::IsCompleted(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    std::scoped_lock lk{applet->lock};

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(broker->IsCompleted());
}

void ILibraryAppletAccessor::GetResult(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(applet->terminate_result);
}

void ILibraryAppletAccessor::PresetLibraryAppletGpuTimeSliceZero(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::Start(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    applet->process->Run();
    FrontendExecute();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::RequestExit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    ASSERT(applet != nullptr);
    applet->message_queue.RequestExit();
    FrontendRequestExit();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::PushInData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    broker->GetInData().Push(rp.PopIpcInterface<IStorage>().lock());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::PopOutData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    std::shared_ptr<IStorage> data;
    const auto res = broker->GetOutData().Pop(&data);

    if (res.IsSuccess()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        rb.PushIpcInterface(std::move(data));
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }
}

void ILibraryAppletAccessor::PushInteractiveInData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    broker->GetInteractiveInData().Push(rp.PopIpcInterface<IStorage>().lock());
    FrontendExecuteInteractive();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::PopInteractiveOutData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    std::shared_ptr<IStorage> data;
    const auto res = broker->GetInteractiveOutData().Pop(&data);

    if (res.IsSuccess()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        rb.PushIpcInterface(std::move(data));
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }
}

void ILibraryAppletAccessor::GetPopOutDataEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetOutData().GetEvent());
}

void ILibraryAppletAccessor::GetPopInteractiveOutDataEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetInteractiveOutData().GetEvent());
}

void ILibraryAppletAccessor::GetIndirectLayerConsumerHandle(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    // We require a non-zero handle to be valid. Using 0xdeadbeef allows us to trace if this is
    // actually used anywhere
    constexpr u64 handle = 0xdeadbeef;

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(handle);
}

void ILibraryAppletAccessor::FrontendExecute() {
    if (applet->frontend) {
        applet->frontend->Initialize();
        applet->frontend->Execute();
    }
}

void ILibraryAppletAccessor::FrontendExecuteInteractive() {
    if (applet->frontend) {
        applet->frontend->ExecuteInteractive();
        applet->frontend->Execute();
    }
}

void ILibraryAppletAccessor::FrontendRequestExit() {
    if (applet->frontend) {
        applet->frontend->RequestExit();
    }
}

} // namespace Service::AM
