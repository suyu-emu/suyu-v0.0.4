// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core_timing.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/am_results.h"
#include "core/hle/service/am/applet_data_broker.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applet_cabinet.h"
#include "core/hle/service/am/frontend/applet_controller.h"
#include "core/hle/service/am/frontend/applet_mii_edit_types.h"
#include "core/hle/service/am/frontend/applet_software_keyboard_types.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/library_applet_self_accessor.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"
#include "hid_core/hid_types.h"

namespace Service::AM {

namespace {

struct AppletIdentityInfo {
    AppletId applet_id;
    INSERT_PADDING_BYTES(0x4);
    u64 application_id;
};
static_assert(sizeof(AppletIdentityInfo) == 0x10, "AppletIdentityInfo has incorrect size.");

AppletIdentityInfo GetCallerIdentity(std::shared_ptr<Applet> applet) {
    if (const auto caller_applet = applet->caller_applet.lock(); caller_applet) {
        // TODO: is this actually the application ID?
        return {
            .applet_id = applet->applet_id,
            .application_id = applet->program_id,
        };
    } else {
        return {
            .applet_id = AppletId::QLaunch,
            .application_id = 0x0100000000001000ull,
        };
    }
}

} // namespace

ILibraryAppletSelfAccessor::ILibraryAppletSelfAccessor(Core::System& system_,
                                                       std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "ILibraryAppletSelfAccessor"}, applet{std::move(applet_)},
      broker{applet->caller_applet_broker} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &ILibraryAppletSelfAccessor::PopInData, "PopInData"},
        {1, &ILibraryAppletSelfAccessor::PushOutData, "PushOutData"},
        {2, &ILibraryAppletSelfAccessor::PopInteractiveInData, "PopInteractiveInData"},
        {3, &ILibraryAppletSelfAccessor::PushInteractiveOutData, "PushInteractiveOutData"},
        {5, &ILibraryAppletSelfAccessor::GetPopInDataEvent, "GetPopInDataEvent"},
        {6, &ILibraryAppletSelfAccessor::GetPopInteractiveInDataEvent, "GetPopInteractiveInDataEvent"},
        {10, &ILibraryAppletSelfAccessor::ExitProcessAndReturn, "ExitProcessAndReturn"},
        {11, &ILibraryAppletSelfAccessor::GetLibraryAppletInfo, "GetLibraryAppletInfo"},
        {12, &ILibraryAppletSelfAccessor::GetMainAppletIdentityInfo, "GetMainAppletIdentityInfo"},
        {13, nullptr, "CanUseApplicationCore"},
        {14, &ILibraryAppletSelfAccessor::GetCallerAppletIdentityInfo, "GetCallerAppletIdentityInfo"},
        {15, nullptr, "GetMainAppletApplicationControlProperty"},
        {16, nullptr, "GetMainAppletStorageId"},
        {17, nullptr, "GetCallerAppletIdentityInfoStack"},
        {18, nullptr, "GetNextReturnDestinationAppletIdentityInfo"},
        {19, &ILibraryAppletSelfAccessor::GetDesirableKeyboardLayout, "GetDesirableKeyboardLayout"},
        {20, nullptr, "PopExtraStorage"},
        {25, nullptr, "GetPopExtraStorageEvent"},
        {30, nullptr, "UnpopInData"},
        {31, nullptr, "UnpopExtraStorage"},
        {40, nullptr, "GetIndirectLayerProducerHandle"},
        {50, nullptr, "ReportVisibleError"},
        {51, nullptr, "ReportVisibleErrorWithErrorContext"},
        {60, nullptr, "GetMainAppletApplicationDesiredLanguage"},
        {70, nullptr, "GetCurrentApplicationId"},
        {80, nullptr, "RequestExitToSelf"},
        {90, nullptr, "CreateApplicationAndPushAndRequestToLaunch"},
        {100, nullptr, "CreateGameMovieTrimmer"},
        {101, nullptr, "ReserveResourceForMovieOperation"},
        {102, nullptr, "UnreserveResourceForMovieOperation"},
        {110, &ILibraryAppletSelfAccessor::GetMainAppletAvailableUsers, "GetMainAppletAvailableUsers"},
        {120, nullptr, "GetLaunchStorageInfoForDebug"},
        {130, nullptr, "GetGpuErrorDetectedSystemEvent"},
        {140, nullptr, "SetApplicationMemoryReservation"},
        {150, &ILibraryAppletSelfAccessor::ShouldSetGpuTimeSliceManually, "ShouldSetGpuTimeSliceManually"},
    };
    // clang-format on
    RegisterHandlers(functions);
}

ILibraryAppletSelfAccessor::~ILibraryAppletSelfAccessor() = default;

void ILibraryAppletSelfAccessor::PopInData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    std::shared_ptr<IStorage> data;
    const auto res = broker->GetInData().Pop(&data);

    if (res.IsSuccess()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        rb.PushIpcInterface(std::move(data));
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }
}

void ILibraryAppletSelfAccessor::PushOutData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    broker->GetOutData().Push(rp.PopIpcInterface<IStorage>().lock());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::PopInteractiveInData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    std::shared_ptr<IStorage> data;
    const auto res = broker->GetInteractiveInData().Pop(&data);

    if (res.IsSuccess()) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(res);
        rb.PushIpcInterface(std::move(data));
    } else {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(res);
    }
}

void ILibraryAppletSelfAccessor::PushInteractiveOutData(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::RequestParser rp{ctx};
    broker->GetInteractiveOutData().Push(rp.PopIpcInterface<IStorage>().lock());

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::GetPopInDataEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetInData().GetEvent());
}

void ILibraryAppletSelfAccessor::GetPopInteractiveInDataEvent(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(broker->GetInteractiveInData().GetEvent());
}

void ILibraryAppletSelfAccessor::ExitProcessAndReturn(HLERequestContext& ctx) {
    LOG_INFO(Service_AM, "called");

    system.GetAppletManager().TerminateAndRemoveApplet(applet->aruid);
    broker->SignalCompletion();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void ILibraryAppletSelfAccessor::GetLibraryAppletInfo(HLERequestContext& ctx) {
    struct LibraryAppletInfo {
        AppletId applet_id;
        LibraryAppletMode library_applet_mode;
    };

    LOG_WARNING(Service_AM, "(STUBBED) called");

    const LibraryAppletInfo applet_info{
        .applet_id = applet->applet_id,
        .library_applet_mode = applet->library_applet_mode,
    };

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::GetMainAppletIdentityInfo(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    const AppletIdentityInfo applet_info{
        .applet_id = AppletId::QLaunch,
        .application_id = 0x0100000000001000ull,
    };

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(applet_info);
}

void ILibraryAppletSelfAccessor::GetCallerAppletIdentityInfo(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(GetCallerIdentity(applet));
}

void ILibraryAppletSelfAccessor::GetDesirableKeyboardLayout(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(0);
}

void ILibraryAppletSelfAccessor::GetMainAppletAvailableUsers(HLERequestContext& ctx) {
    const Service::Account::ProfileManager manager{};
    bool is_empty{true};
    s32 user_count{-1};

    LOG_INFO(Service_AM, "called");

    if (manager.GetUserCount() > 0) {
        is_empty = false;
        user_count = static_cast<s32>(manager.GetUserCount());
        ctx.WriteBuffer(manager.GetAllUsers());
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u8>(is_empty);
    rb.Push(user_count);
}

void ILibraryAppletSelfAccessor::ShouldSetGpuTimeSliceManually(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u8>(0);
}

} // namespace Service::AM
