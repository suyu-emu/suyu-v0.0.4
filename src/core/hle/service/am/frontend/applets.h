// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <queue>

#include "common/swap.h"
#include "core/hle/service/am/applet.h"

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

namespace Frontend {

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

class FrontendApplet {
public:
    explicit FrontendApplet(Core::System& system_, LibraryAppletMode applet_mode_);
    virtual ~FrontendApplet();

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

struct FrontendAppletSet {
    using CabinetApplet = std::unique_ptr<Core::Frontend::CabinetApplet>;
    using ControllerApplet = std::unique_ptr<Core::Frontend::ControllerApplet>;
    using ErrorApplet = std::unique_ptr<Core::Frontend::ErrorApplet>;
    using MiiEdit = std::unique_ptr<Core::Frontend::MiiEditApplet>;
    using ParentalControlsApplet = std::unique_ptr<Core::Frontend::ParentalControlsApplet>;
    using PhotoViewer = std::unique_ptr<Core::Frontend::PhotoViewerApplet>;
    using ProfileSelect = std::unique_ptr<Core::Frontend::ProfileSelectApplet>;
    using SoftwareKeyboard = std::unique_ptr<Core::Frontend::SoftwareKeyboardApplet>;
    using WebBrowser = std::unique_ptr<Core::Frontend::WebBrowserApplet>;

    FrontendAppletSet();
    FrontendAppletSet(CabinetApplet cabinet_applet, ControllerApplet controller_applet,
                      ErrorApplet error_applet, MiiEdit mii_edit_,
                      ParentalControlsApplet parental_controls_applet, PhotoViewer photo_viewer_,
                      ProfileSelect profile_select_, SoftwareKeyboard software_keyboard_,
                      WebBrowser web_browser_);
    ~FrontendAppletSet();

    FrontendAppletSet(const FrontendAppletSet&) = delete;
    FrontendAppletSet& operator=(const FrontendAppletSet&) = delete;

    FrontendAppletSet(FrontendAppletSet&&) noexcept;
    FrontendAppletSet& operator=(FrontendAppletSet&&) noexcept;

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

class FrontendAppletHolder {
public:
    explicit FrontendAppletHolder(Core::System& system_);
    ~FrontendAppletHolder();

    const FrontendAppletSet& GetFrontendAppletSet() const;
    NFP::CabinetMode GetCabinetMode() const;
    AppletId GetCurrentAppletId() const;

    void SetFrontendAppletSet(FrontendAppletSet set);
    void SetCabinetMode(NFP::CabinetMode mode);
    void SetCurrentAppletId(AppletId applet_id);
    void SetDefaultAppletFrontendSet();
    void SetDefaultAppletsIfMissing();
    void ClearAll();

    std::shared_ptr<FrontendApplet> GetApplet(AppletId id, LibraryAppletMode mode) const;

private:
    AppletId current_applet_id{};
    NFP::CabinetMode cabinet_mode{};

    FrontendAppletSet frontend;
    Core::System& system;
};

} // namespace Frontend
} // namespace Service::AM
