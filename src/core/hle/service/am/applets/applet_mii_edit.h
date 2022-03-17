// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/result.h"
#include "core/hle/service/am/applets/applet_mii_edit_types.h"
#include "core/hle/service/am/applets/applets.h"

namespace Core {
class System;
} // namespace Core

namespace Service::AM::Applets {

class MiiEdit final : public Applet {
public:
    explicit MiiEdit(Core::System& system_, LibraryAppletMode applet_mode_,
                     const Core::Frontend::MiiEditApplet& frontend_);
    ~MiiEdit() override;

    void Initialize() override;

    bool TransactionComplete() const override;
    ResultCode GetStatus() const override;
    void ExecuteInteractive() override;
    void Execute() override;

    void DisplayCompleted(const Core::Frontend::MiiParameters& parameters);

private:
    const Core::Frontend::MiiEditApplet& frontend;
    Core::System& system;

    MiiAppletInput input_data{};
    AppletOutputForCharInfoEditing output_data{};

    bool is_complete{false};
};

} // namespace Service::AM::Applets
