// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class IWindowController final : public ServiceFramework<IWindowController> {
public:
    explicit IWindowController(Core::System& system_, std::shared_ptr<Applet> applet);
    ~IWindowController() override;

private:
    Result GetAppletResourceUserId(Out<AppletResourceUserId> out_aruid);
    Result GetAppletResourceUserIdOfCallerApplet(Out<AppletResourceUserId> out_aruid);
    Result AcquireForegroundRights();
    Result ReleaseForegroundRights();
    Result RejectToChangeIntoBackground();
    Result SetAppletWindowVisibility(bool visible);
    Result SetAppletGpuTimeSlice(s64 time_slice);

    const std::shared_ptr<Applet> m_applet;
};

} // namespace Service::AM
