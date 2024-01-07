// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class IWindowController final : public ServiceFramework<IWindowController> {
public:
    explicit IWindowController(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~IWindowController() override;

private:
    void GetAppletResourceUserId(HLERequestContext& ctx);
    void GetAppletResourceUserIdOfCallerApplet(HLERequestContext& ctx);
    void AcquireForegroundRights(HLERequestContext& ctx);
    void SetAppletWindowVisibility(HLERequestContext& ctx);
    void SetAppletGpuTimeSlice(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
