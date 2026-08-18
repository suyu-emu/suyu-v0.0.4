// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/result.h"
#include "core/hle/service/tma/tma.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"
#include "core/hle/service/server_manager.h"

namespace Service::TMA {

class HTC_TENV final : public ServiceFramework<HTC_TENV> {
public:
    explicit HTC_TENV(Core::System& system_)
        : ServiceFramework{system_, "htc:tenv"}
    {
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetServiceInterface"},
        };
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);
    server_manager->RegisterNamedService("htc:tenv", std::make_shared<HTC_TENV>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::TMA
