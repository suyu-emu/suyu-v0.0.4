// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/settings.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/service/am/applet_data_broker.h"
#include "core/hle/service/am/applet_manager.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/am/library_applet_accessor.h"
#include "core/hle/service/am/library_applet_creator.h"
#include "core/hle/service/am/library_applet_storage.h"
#include "core/hle/service/am/storage.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/sm/sm.h"

namespace Service::AM {

namespace {

bool ShouldCreateGuestApplet(AppletId applet_id) {
#define X(Name, name)                                                                              \
    if (applet_id == AppletId::Name &&                                                             \
        Settings::values.name##_applet_mode.GetValue() != Settings::AppletMode::LLE) {             \
        return false;                                                                              \
    }

    X(Cabinet, cabinet)
    X(Controller, controller)
    X(DataErase, data_erase)
    X(Error, error)
    X(NetConnect, net_connect)
    X(ProfileSelect, player_select)
    X(SoftwareKeyboard, swkbd)
    X(MiiEdit, mii_edit)
    X(Web, web)
    X(Shop, shop)
    X(PhotoViewer, photo_viewer)
    X(OfflineWeb, offline_web)
    X(LoginShare, login_share)
    X(WebAuth, wifi_web_auth)
    X(MyPage, my_page)

#undef X

    return true;
}

AppletProgramId AppletIdToProgramId(AppletId applet_id) {
    switch (applet_id) {
    case AppletId::OverlayDisplay:
        return AppletProgramId::OverlayDisplay;
    case AppletId::QLaunch:
        return AppletProgramId::QLaunch;
    case AppletId::Starter:
        return AppletProgramId::Starter;
    case AppletId::Auth:
        return AppletProgramId::Auth;
    case AppletId::Cabinet:
        return AppletProgramId::Cabinet;
    case AppletId::Controller:
        return AppletProgramId::Controller;
    case AppletId::DataErase:
        return AppletProgramId::DataErase;
    case AppletId::Error:
        return AppletProgramId::Error;
    case AppletId::NetConnect:
        return AppletProgramId::NetConnect;
    case AppletId::ProfileSelect:
        return AppletProgramId::ProfileSelect;
    case AppletId::SoftwareKeyboard:
        return AppletProgramId::SoftwareKeyboard;
    case AppletId::MiiEdit:
        return AppletProgramId::MiiEdit;
    case AppletId::Web:
        return AppletProgramId::Web;
    case AppletId::Shop:
        return AppletProgramId::Shop;
    case AppletId::PhotoViewer:
        return AppletProgramId::PhotoViewer;
    case AppletId::Settings:
        return AppletProgramId::Settings;
    case AppletId::OfflineWeb:
        return AppletProgramId::OfflineWeb;
    case AppletId::LoginShare:
        return AppletProgramId::LoginShare;
    case AppletId::WebAuth:
        return AppletProgramId::WebAuth;
    case AppletId::MyPage:
        return AppletProgramId::MyPage;
    default:
        return static_cast<AppletProgramId>(0);
    }
}

std::shared_ptr<ILibraryAppletAccessor> CreateGuestApplet(Core::System& system,
                                                          std::shared_ptr<Applet> caller_applet,
                                                          AppletId applet_id,
                                                          LibraryAppletMode mode) {
    const auto program_id = static_cast<u64>(AppletIdToProgramId(applet_id));
    if (program_id == 0) {
        // Unknown applet
        return {};
    }

    // TODO: enable other versions of applets
    enum : u8 {
        Firmware1400 = 14,
        Firmware1500 = 15,
        Firmware1600 = 16,
        Firmware1700 = 17,
    };

    auto process = std::make_unique<Process>(system);
    if (!process->Initialize(program_id, Firmware1400, Firmware1700)) {
        // Couldn't initialize the guest process
        return {};
    }

    const auto applet = std::make_shared<Applet>(system, std::move(process));
    applet->program_id = program_id;
    applet->applet_id = applet_id;
    applet->type = AppletType::LibraryApplet;
    applet->library_applet_mode = mode;

    // Set focus state
    switch (mode) {
    case LibraryAppletMode::AllForeground:
    case LibraryAppletMode::NoUi:
    case LibraryAppletMode::PartialForeground:
    case LibraryAppletMode::PartialForegroundIndirectDisplay:
        applet->hid_registration.EnableAppletToGetInput(true);
        applet->focus_state = FocusState::InFocus;
        applet->message_queue.PushMessage(AppletMessageQueue::AppletMessage::ChangeIntoForeground);
        break;
    case LibraryAppletMode::AllForegroundInitiallyHidden:
        applet->hid_registration.EnableAppletToGetInput(false);
        applet->focus_state = FocusState::NotInFocus;
        applet->system_buffer_manager.SetWindowVisibility(false);
        applet->message_queue.PushMessage(AppletMessageQueue::AppletMessage::ChangeIntoBackground);
        break;
    }

    auto broker = std::make_shared<AppletDataBroker>(system);
    applet->caller_applet = caller_applet;
    applet->caller_applet_broker = broker;

    system.GetAppletManager().InsertApplet(applet);

    return std::make_shared<ILibraryAppletAccessor>(system, broker, applet);
}

std::shared_ptr<ILibraryAppletAccessor> CreateFrontendApplet(Core::System& system,
                                                             std::shared_ptr<Applet> caller_applet,
                                                             AppletId applet_id,
                                                             LibraryAppletMode mode) {
    const auto program_id = static_cast<u64>(AppletIdToProgramId(applet_id));

    auto process = std::make_unique<Process>(system);
    auto applet = std::make_shared<Applet>(system, std::move(process));
    applet->program_id = program_id;
    applet->applet_id = applet_id;
    applet->type = AppletType::LibraryApplet;
    applet->library_applet_mode = mode;

    auto storage = std::make_shared<AppletDataBroker>(system);
    applet->caller_applet = caller_applet;
    applet->caller_applet_broker = storage;
    applet->frontend = system.GetFrontendAppletHolder().GetApplet(applet, applet_id, mode);

    return std::make_shared<ILibraryAppletAccessor>(system, storage, applet);
}

} // namespace

ILibraryAppletCreator::ILibraryAppletCreator(Core::System& system_, std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "ILibraryAppletCreator"}, applet{std::move(applet_)} {
    static const FunctionInfo functions[] = {
        {0, &ILibraryAppletCreator::CreateLibraryApplet, "CreateLibraryApplet"},
        {1, nullptr, "TerminateAllLibraryApplets"},
        {2, nullptr, "AreAnyLibraryAppletsLeft"},
        {10, &ILibraryAppletCreator::CreateStorage, "CreateStorage"},
        {11, &ILibraryAppletCreator::CreateTransferMemoryStorage, "CreateTransferMemoryStorage"},
        {12, &ILibraryAppletCreator::CreateHandleStorage, "CreateHandleStorage"},
    };
    RegisterHandlers(functions);
}

ILibraryAppletCreator::~ILibraryAppletCreator() = default;

void ILibraryAppletCreator::CreateLibraryApplet(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const auto applet_id = rp.PopRaw<AppletId>();
    const auto applet_mode = rp.PopRaw<LibraryAppletMode>();

    LOG_DEBUG(Service_AM, "called with applet_id={:08X}, applet_mode={:08X}", applet_id,
              applet_mode);

    std::shared_ptr<ILibraryAppletAccessor> library_applet;
    if (ShouldCreateGuestApplet(applet_id)) {
        library_applet = CreateGuestApplet(system, applet, applet_id, applet_mode);
    }
    if (!library_applet) {
        library_applet = CreateFrontendApplet(system, applet, applet_id, applet_mode);
    }
    if (!library_applet) {
        LOG_ERROR(Service_AM, "Applet doesn't exist! applet_id={}", applet_id);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    // Applet is created, can now be launched.
    applet->library_applet_launchable_event.Signal();

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<ILibraryAppletAccessor>(library_applet);
}

void ILibraryAppletCreator::CreateStorage(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const s64 size{rp.Pop<s64>()};

    LOG_DEBUG(Service_AM, "called, size={}", size);

    if (size <= 0) {
        LOG_ERROR(Service_AM, "size is less than or equal to 0");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    std::vector<u8> data(size);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(system, AM::CreateStorage(std::move(data)));
}

void ILibraryAppletCreator::CreateTransferMemoryStorage(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    struct Parameters {
        bool is_writable;
        s64 size;
    };

    const auto params{rp.PopRaw<Parameters>()};
    const auto handle{ctx.GetCopyHandle(0)};

    LOG_DEBUG(Service_AM, "called, is_writable={}, size={}, handle={:08X}", params.is_writable,
              params.size, handle);

    if (params.size <= 0) {
        LOG_ERROR(Service_AM, "size is less than or equal to 0");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto transfer_mem = ctx.GetObjectFromHandle<Kernel::KTransferMemory>(handle);

    if (transfer_mem.IsNull()) {
        LOG_ERROR(Service_AM, "transfer_mem is a nullptr for handle={:08X}", handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(
        system, AM::CreateTransferMemoryStorage(ctx.GetMemory(), transfer_mem.GetPointerUnsafe(),
                                                params.is_writable, params.size));
}

void ILibraryAppletCreator::CreateHandleStorage(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    const s64 size{rp.Pop<s64>()};
    const auto handle{ctx.GetCopyHandle(0)};

    LOG_DEBUG(Service_AM, "called, size={}, handle={:08X}", size, handle);

    if (size <= 0) {
        LOG_ERROR(Service_AM, "size is less than or equal to 0");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto transfer_mem = ctx.GetObjectFromHandle<Kernel::KTransferMemory>(handle);

    if (transfer_mem.IsNull()) {
        LOG_ERROR(Service_AM, "transfer_mem is a nullptr for handle={:08X}", handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IStorage>(
        system, AM::CreateHandleStorage(ctx.GetMemory(), transfer_mem.GetPointerUnsafe(), size));
}

} // namespace Service::AM
