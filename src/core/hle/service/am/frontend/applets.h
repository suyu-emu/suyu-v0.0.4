// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <queue>

#include "common/swap.h"
#include "core/hle/service/am/applet.h"
#include "core/frontend/applets/net_connect.h"

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

class FrontendApplet {
public:
    explicit FrontendApplet(Core::System& system_, std::shared_ptr<Applet> applet_,
                            LibraryAppletMode applet_mode_);
    virtual ~FrontendApplet();

    virtual void Initialize();

    virtual Result GetStatus() const = 0;
    virtual void ExecuteInteractive() = 0;
    virtual void Execute() = 0;
    virtual Result RequestExit() = 0;

    LibraryAppletMode GetLibraryAppletMode() const {
        return applet_mode;
    }

    bool IsInitialized() const {
        return initialized;
    }

protected:
    std::shared_ptr<IStorage> PopInData();
    std::shared_ptr<IStorage> PopInteractiveInData();
    void PushOutData(std::shared_ptr<IStorage> storage);
    void PushInteractiveOutData(std::shared_ptr<IStorage> storage);
    void Exit();

protected:
    Core::System& system;
    CommonArguments common_args{};
    std::weak_ptr<Applet> applet{};
    LibraryAppletMode applet_mode{};
    bool initialized{false};
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
    using NetConnect = std::unique_ptr<Core::Frontend::NetConnectApplet>;

    FrontendAppletSet();
    FrontendAppletSet(CabinetApplet cabinet_applet, ControllerApplet controller_applet,
                      ErrorApplet error_applet, MiiEdit mii_edit_,
                      ParentalControlsApplet parental_controls_applet, PhotoViewer photo_viewer_,
                      ProfileSelect profile_select_, SoftwareKeyboard software_keyboard_,
                      WebBrowser web_browser_, NetConnect net_connect_);
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
    NetConnect net_connect;
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
    void SetDefaultAppletsIfMissing();
    void ClearAll();

    std::shared_ptr<FrontendApplet> GetApplet(std::shared_ptr<Applet> applet, AppletId id,
                                              LibraryAppletMode mode) const;

private:
    AppletId current_applet_id{};
    NFP::CabinetMode cabinet_mode{};

    FrontendAppletSet frontend;
    Core::System& system;
};

} // namespace Frontend
} // namespace Service::AM
