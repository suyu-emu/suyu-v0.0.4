// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/am/applets/applets.h"

namespace Core {
class System;
}

namespace Service::AM::Applets {

class WebBrowser final : public Applet {
public:
    WebBrowser(Core::System& system_, const Core::Frontend::WebBrowserApplet& frontend_);

    ~WebBrowser() override;

    void Initialize() override;

    bool TransactionComplete() const override;
    ResultCode GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;

private:
    const Core::Frontend::WebBrowserApplet& frontend;

    bool complete{false};
    ResultCode status{RESULT_SUCCESS};

    Core::System& system;
};

} // namespace Service::AM::Applets
