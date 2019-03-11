// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/am/applets/applets.h"

namespace Service::AM::Applets {

union ErrorArguments;

enum class ErrorAppletMode : u8 {
    ShowError = 0,
    ShowSystemError = 1,
    ShowApplicationError = 2,
    ShowEula = 3,
    ShowErrorPctl = 4,
    ShowErrorRecord = 5,
    ShowUpdateEula = 8,
};

class Error final : public Applet {
public:
    Error(const Core::Frontend::ErrorApplet& frontend);
    ~Error() override;

    void Initialize() override;

    bool TransactionComplete() const override;
    ResultCode GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;

    void DisplayCompleted();

private:
    const Core::Frontend::ErrorApplet& frontend;
    ResultCode error_code = RESULT_SUCCESS;
    ErrorAppletMode mode;
    std::unique_ptr<ErrorArguments> args;

    bool complete;
};

} // namespace Service::AM::Applets
