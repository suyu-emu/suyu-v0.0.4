// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/power_state_request_manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class IPowerStateRequestHandler final : public ServiceFramework<IPowerStateRequestHandler> {
public:
    explicit IPowerStateRequestHandler(Core::System& system,
                                       PowerStateRequestManager& power_state_request_manager);

    ~IPowerStateRequestHandler() override = default;

private:
    void GetPowerStateRequestEventReadableHandle(HLERequestContext& ctx);
    void GetAndClearPowerStateRequest(HLERequestContext& ctx);

    Core::System& m_system;
    PowerStateRequestManager& m_power_state_request_manager;
};

} // namespace Service::PSC::Time
