// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <memory>

#include "core/core.h"
#include "core/hle/service/ptm/psm.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ts.h"
#include "core/hle/service/server_manager.h"

namespace Service::PTM {

class PSM_MANU final : public ServiceFramework<PSM_MANU> {
public:
    explicit PSM_MANU(Core::System& system_)
        : ServiceFramework{system_, "psm:manu"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "EnableVdd50StateControl"},
            {1, nullptr, "DisableVdd50StateControl"},
            {2, nullptr, "SetVdd50State"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class POWCTL final : public ServiceFramework<POWCTL> {
public:
    explicit POWCTL(Core::System& system_)
        : ServiceFramework{system_, "powctl"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "OpenSession"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("psm", std::make_shared<PSM>(system));
    if (1 /* not retail */) {
        server_manager->RegisterNamedService("psm:manu", std::make_shared<PSM_MANU>(system));
        server_manager->RegisterNamedService("powctl", std::make_shared<POWCTL>(system));
    }
    server_manager->RegisterNamedService("ts", std::make_shared<TS>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::PTM
