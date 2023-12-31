// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/library_applet_accessor.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

ILibraryAppletAccessor::ILibraryAppletAccessor(Core::System& system_,
                                               std::shared_ptr<Frontend::FrontendApplet> applet_)
    : ServiceFramework{system_, "ILibraryAppletAccessor"}, applet{std::move(applet_)} {
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

void ILibraryAppletAccessor::GetAppletStateChangedEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->GetBroker().GetStateChangedEvent());
}

void ILibraryAppletAccessor::IsCompleted(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(applet->TransactionComplete());
}

void ILibraryAppletAccessor::GetResult(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(applet->GetStatus());
}

void ILibraryAppletAccessor::PresetLibraryAppletGpuTimeSliceZero(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::Start(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    ASSERT(applet != nullptr);

    applet->Initialize();
    applet->Execute();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::RequestExit(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    ASSERT(applet != nullptr);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(applet->RequestExit());
}

void ILibraryAppletAccessor::PushInData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    applet->GetBroker().PushNormalDataFromGame(rp.PopIpcInterface<IStorage>().lock());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::PopOutData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    auto storage = applet->GetBroker().PopNormalDataToGame();
    if (storage == nullptr) {
        LOG_DEBUG(Service_AM,
                  "storage is a nullptr. There is no data in the current normal channel");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultNoDataInChannel);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(std::move(storage));
}

void ILibraryAppletAccessor::PushInteractiveInData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    applet->GetBroker().PushInteractiveDataFromGame(rp.PopIpcInterface<IStorage>().lock());

    ASSERT(applet->IsInitialized());
    applet->ExecuteInteractive();
    applet->Execute();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletAccessor::PopInteractiveOutData(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    auto storage = applet->GetBroker().PopInteractiveDataToGame();
    if (storage == nullptr) {
        LOG_DEBUG(Service_AM,
                  "storage is a nullptr. There is no data in the current interactive channel");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(AM::ResultNoDataInChannel);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(std::move(storage));
}

void ILibraryAppletAccessor::GetPopOutDataEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->GetBroker().GetNormalDataEvent());
}

void ILibraryAppletAccessor::GetPopInteractiveOutDataEvent(HLERequestContext& ctx) {
    LOG_DEBUG(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(applet->GetBroker().GetInteractiveDataEvent());
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

} // namespace Service::AM
