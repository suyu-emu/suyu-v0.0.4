// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class IWindowController final : public ServiceFramework<IWindowController> {
public:
    explicit IWindowController(Core::System& system_);
    ~IWindowController() override;

private:
    void GetAppletResourceUserId(HLERequestContext& ctx);
    void GetAppletResourceUserIdOfCallerApplet(HLERequestContext& ctx);
    void AcquireForegroundRights(HLERequestContext& ctx);
};

} // namespace Service::AM
