// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class IProcessWindingController final : public ServiceFramework<IProcessWindingController> {
public:
    explicit IProcessWindingController(Core::System& system_);
    ~IProcessWindingController() override;

private:
    void GetLaunchReason(HLERequestContext& ctx);
    void OpenCallingLibraryApplet(HLERequestContext& ctx);
};

} // namespace Service::AM
