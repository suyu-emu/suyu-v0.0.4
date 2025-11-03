// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::LDN {

class IClientProcessMonitor final
    : public ServiceFramework<IClientProcessMonitor> {
public:
    explicit IClientProcessMonitor(Core::System& system_);
    ~IClientProcessMonitor() override;

private:
    Result RegisterClient();
};

} // namespace Service::LDN
