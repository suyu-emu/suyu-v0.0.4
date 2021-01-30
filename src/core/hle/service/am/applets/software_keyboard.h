// Copyright 2018 yuzu emulator team
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

class SoftwareKeyboard final : public Applet {
public:
    explicit SoftwareKeyboard(Core::System& system_,
                              const Core::Frontend::SoftwareKeyboardApplet& frontend_);
    ~SoftwareKeyboard() override;

    void Initialize() override;

    bool TransactionComplete() const override;
    ResultCode GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;

private:
    const Core::Frontend::SoftwareKeyboardApplet& frontend;

    Core::System& system;
};

} // namespace Service::AM::Applets
