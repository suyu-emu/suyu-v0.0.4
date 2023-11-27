// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <queue>

#include "common/swap.h"
#include "core/hle/service/kernel_helpers.h"

union Result;

namespace Core {
class System;
}

namespace Core::Frontend {
class CabinetApplet;
class ControllerApplet;
class ECommerceApplet;
class ErrorApplet;
class MiiEditApplet;
class ParentalControlsApplet;
class PhotoViewerApplet;
class ProfileSelectApplet;
class SoftwareKeyboardApplet;
class WebBrowserApplet;
} // namespace Core::Frontend

namespace Kernel {
class KernelCore;
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service::NFP {
enum class CabinetMode : u8;
} // namespace Service::NFP

namespace Service::AM {

class IStorage;

namespace Applets {

enum class AppletId : u32 {
    None = 0x00,
    Application = 0x01,
    OverlayDisplay = 0x02,
    QLaunch = 0x03,
    Starter = 0x04,
    Auth = 0x0A,
    Cabinet = 0x0B,
    Controller = 0x0C,
    DataErase = 0x0D,
    Error = 0x0E,
    NetConnect = 0x0F,
    ProfileSelect = 0x10,
    SoftwareKeyboard = 0x11,
    MiiEdit = 0x12,
    Web = 0x13,
    Shop = 0x14,
    PhotoViewer = 0x15,
    Settings = 0x16,
    OfflineWeb = 0x17,
    LoginShare = 0x18,
    WebAuth = 0x19,
    MyPage = 0x1A,
};

enum class AppletProgramId : u64 {
    QLaunch = 0x0100000000001000ull,
    Auth = 0x0100000000001001ull,
    Cabinet = 0x0100000000001002ull,
    Controller = 0x0100000000001003ull,
    DataErase = 0x0100000000001004ull,
    Error = 0x0100000000001005ull,
    NetConnect = 0x0100000000001006ull,
    ProfileSelect = 0x0100000000001007ull,
    SoftwareKeyboard = 0x0100000000001008ull,
    MiiEdit = 0x0100000000001009ull,
    Web = 0x010000000000100Aull,
    Shop = 0x010000000000100Bull,
    OverlayDisplay = 0x010000000000100Cull,
    PhotoViewer = 0x010000000000100Dull,
    Settings = 0x010000000000100Eull,
    OfflineWeb = 0x010000000000100Full,
    LoginShare = 0x0100000000001010ull,
    WebAuth = 0x0100000000001011ull,
    Starter = 0x0100000000001012ull,
    MyPage = 0x0100000000001013ull,
    MaxProgramId = 0x0100000000001FFFull,
};

enum class LibraryAppletMode : u32 {
    AllForeground = 0,
    Background = 1,
    NoUI = 2,
    BackgroundIndirectDisplay = 3,
    AllForegroundInitiallyHidden = 4,
};

enum class CommonArgumentVersion : u32 {
    Version0,
    Version1,
    Version2,
    Version3,
};

enum class CommonArgumentSize : u32 {
    Version3 = 0x20,
};

enum class ThemeColor : u32 {
    BasicWhite = 0,
    BasicBlack = 3,
};

struct CommonArguments {
    CommonArgumentVersion arguments_version;
    CommonArgumentSize size;
    u32 library_version;
    ThemeColor theme_color;
    bool play_startup_sound;
    u64_le system_tick;
};
static_assert(sizeof(CommonArguments) == 0x20, "CommonArguments has incorrect size.");

class AppletDataBroker final {
public:
    explicit AppletDataBroker(Core::System& system_, LibraryAppletMode applet_mode_);
    ~AppletDataBroker();

    struct RawChannelData {
        std::vector<std::vector<u8>> normal;
        std::vector<std::vector<u8>> interactive;
    };

    // Retrieves but does not pop the data sent to applet.
    RawChannelData PeekDataToAppletForDebug() const;

    std::shared_ptr<IStorage> PopNormalDataToGame();
    std::shared_ptr<IStorage> PopNormalDataToApplet();

    std::shared_ptr<IStorage> PopInteractiveDataToGame();
    std::shared_ptr<IStorage> PopInteractiveDataToApplet();

    void PushNormalDataFromGame(std::shared_ptr<IStorage>&& storage);
    void PushNormalDataFromApplet(std::shared_ptr<IStorage>&& storage);

    void PushInteractiveDataFromGame(std::shared_ptr<IStorage>&& storage);
    void PushInteractiveDataFromApplet(std::shared_ptr<IStorage>&& storage);

    void SignalStateChanged();

    Kernel::KReadableEvent& GetNormalDataEvent();
    Kernel::KReadableEvent& GetInteractiveDataEvent();
    Kernel::KReadableEvent& GetStateChangedEvent();

private:
    Core::System& system;
    LibraryAppletMode applet_mode;

    KernelHelpers::ServiceContext service_context;

    // Queues are named from applet's perspective

    // PopNormalDataToApplet and PushNormalDataFromGame
    std::deque<std::shared_ptr<IStorage>> in_channel;

    // PopNormalDataToGame and PushNormalDataFromApplet
    std::deque<std::shared_ptr<IStorage>> out_channel;

    // PopInteractiveDataToApplet and PushInteractiveDataFromGame
    std::deque<std::shared_ptr<IStorage>> in_interactive_channel;

    // PopInteractiveDataToGame and PushInteractiveDataFromApplet
    std::deque<std::shared_ptr<IStorage>> out_interactive_channel;

    Kernel::KEvent* state_changed_event;

    // Signaled on PushNormalDataFromApplet
    Kernel::KEvent* pop_out_data_event;

    // Signaled on PushInteractiveDataFromApplet
    Kernel::KEvent* pop_interactive_out_data_event;
};

class Applet {
public:
    explicit Applet(Core::System& system_, LibraryAppletMode applet_mode_);
    virtual ~Applet();

    virtual void Initialize();

    virtual bool TransactionComplete() const = 0;
    virtual Result GetStatus() const = 0;
    virtual void ExecuteInteractive() = 0;
    virtual void Execute() = 0;
    virtual Result RequestExit() = 0;

    AppletDataBroker& GetBroker() {
        return broker;
    }

    const AppletDataBroker& GetBroker() const {
        return broker;
    }

    LibraryAppletMode GetLibraryAppletMode() const {
        return applet_mode;
    }

    bool IsInitialized() const {
        return initialized;
    }

protected:
    CommonArguments common_args{};
    AppletDataBroker broker;
    LibraryAppletMode applet_mode;
    bool initialized = false;
};

struct AppletFrontendSet {
    using CabinetApplet = std::unique_ptr<Core::Frontend::CabinetApplet>;
    using ControllerApplet = std::unique_ptr<Core::Frontend::ControllerApplet>;
    using ErrorApplet = std::unique_ptr<Core::Frontend::ErrorApplet>;
    using MiiEdit = std::unique_ptr<Core::Frontend::MiiEditApplet>;
    using ParentalControlsApplet = std::unique_ptr<Core::Frontend::ParentalControlsApplet>;
    using PhotoViewer = std::unique_ptr<Core::Frontend::PhotoViewerApplet>;
    using ProfileSelect = std::unique_ptr<Core::Frontend::ProfileSelectApplet>;
    using SoftwareKeyboard = std::unique_ptr<Core::Frontend::SoftwareKeyboardApplet>;
    using WebBrowser = std::unique_ptr<Core::Frontend::WebBrowserApplet>;

    AppletFrontendSet();
    AppletFrontendSet(CabinetApplet cabinet_applet, ControllerApplet controller_applet,
                      ErrorApplet error_applet, MiiEdit mii_edit_,
                      ParentalControlsApplet parental_controls_applet, PhotoViewer photo_viewer_,
                      ProfileSelect profile_select_, SoftwareKeyboard software_keyboard_,
                      WebBrowser web_browser_);
    ~AppletFrontendSet();

    AppletFrontendSet(const AppletFrontendSet&) = delete;
    AppletFrontendSet& operator=(const AppletFrontendSet&) = delete;

    AppletFrontendSet(AppletFrontendSet&&) noexcept;
    AppletFrontendSet& operator=(AppletFrontendSet&&) noexcept;

    CabinetApplet cabinet;
    ControllerApplet controller;
    ErrorApplet error;
    MiiEdit mii_edit;
    ParentalControlsApplet parental_controls;
    PhotoViewer photo_viewer;
    ProfileSelect profile_select;
    SoftwareKeyboard software_keyboard;
    WebBrowser web_browser;
};

class AppletManager {
public:
    explicit AppletManager(Core::System& system_);
    ~AppletManager();

    const AppletFrontendSet& GetAppletFrontendSet() const;
    NFP::CabinetMode GetCabinetMode() const;
    AppletId GetCurrentAppletId() const;

    void SetAppletFrontendSet(AppletFrontendSet set);
    void SetCabinetMode(NFP::CabinetMode mode);
    void SetCurrentAppletId(AppletId applet_id);
    void SetDefaultAppletFrontendSet();
    void SetDefaultAppletsIfMissing();
    void ClearAll();

    std::shared_ptr<Applet> GetApplet(AppletId id, LibraryAppletMode mode) const;

private:
    AppletId current_applet_id{};
    NFP::CabinetMode cabinet_mode{};

    AppletFrontendSet frontend;
    Core::System& system;
};

} // namespace Applets
} // namespace Service::AM
