// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::Capture {

class IScreenShotService final : public ServiceFramework<IScreenShotService> {
public:
    explicit IScreenShotService(Core::System& system_);
    ~IScreenShotService() override;
};

} // namespace Service::Capture
