// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/am/frontend/applets.h"

namespace Core {
class System;
}

namespace Service::AM::Frontend {

class NetConnect final : public FrontendApplet {
public:
    explicit NetConnect(Core::System& system_, std::shared_ptr<Applet> applet_,
                         LibraryAppletMode applet_mode_,
                         const Core::Frontend::NetConnectApplet& frontend_);
    ~NetConnect() override;

    void Initialize() override;
    Result GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;
    Result RequestExit() override;

private:
    const Core::Frontend::NetConnectApplet& frontend;
    bool complete = false;
};

} // namespace Service::AM::Frontend
