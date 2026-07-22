// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/gpio/gpio.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/service.h"

namespace Service::GPIO {

class GPIO final : public ServiceFramework<GPIO> {
public:
    explicit GPIO(Core::System& system_)
        : ServiceFramework{system_, "gpio"}
    {
        static const FunctionInfo functions[] = {
            {0, nullptr, "Cmd0"},
        };
        RegisterHandlers(functions);
    }
    ~GPIO() override = default;
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    server_manager->RegisterNamedService("gpio", std::make_shared<GPIO>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::GPIO
