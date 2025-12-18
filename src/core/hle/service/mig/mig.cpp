// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/service/mig/mig.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::Migration {

class MIG_USR final : public ServiceFramework<MIG_USR> {
public:
    explicit MIG_USR(Core::System& system_) : ServiceFramework{system_, "mig:usr"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown0"}, //19.0.0+
            {1, nullptr, "Unknown1"}, //20.0.0+
            {2, nullptr, "Unknown2"}, //20.0.0+
            {10, nullptr, "TryGetLastMigrationInfo"},
            {11, nullptr, "Unknown11"}, //20.0.0+
            {100, nullptr, "CreateUserMigrationServer"}, //7.0.0+
            {101, nullptr, "ResumeUserMigrationServer"}, //7.0.0+
            {200, nullptr, "CreateUserMigrationClient"}, //7.0.0+
            {201, nullptr, "ResumeUserMigrationClient"}, //7.0.0+
            {1001, nullptr, "GetSaveDataMigrationPolicyInfoAsync"}, //8.0.0-20.5.0
            {1010, nullptr, "TryGetLastSaveDataMigrationInfo"}, //7.0.0+
            {1100, nullptr, "CreateSaveDataMigrationServer"}, //7.0.0-19.0.1
            {1101, nullptr, "ResumeSaveDataMigrationServer"}, //7.0.0+
            {1110, nullptr, "Unknown1101"}, //17.0.0+
            {1200, nullptr, "CreateSaveDataMigrationClient"}, //7.0.0+
            {1201, nullptr, "ResumeSaveDataMigrationClient"}, //7.0.0+
            {2001, nullptr, "Unknown2001"}, //20.0.0+
            {2010, nullptr, "Unknown2010"}, //20.0.0+
            {2100, nullptr, "Unknown2100"}, //20.0.0+
            {2110, nullptr, "Unknown2110"}, //20.0.0+
            {2200, nullptr, "Unknown2200"}, //20.0.0+
            {2210, nullptr, "Unknown2210"}, //20.0.0+
            {2220, nullptr, "Unknown2220"}, //20.0.0+
            {2230, nullptr, "Unknown2230"}, //20.0.0+
            {2231, nullptr, "Unknown2231"}, //20.0.0+
            {2232, nullptr, "Unknown2232"}, //20.0.0+
            {2233, nullptr, "Unknown2233"}, //20.0.0+
            {2234, nullptr, "Unknown2234"}, //20.0.0+
            {2250, nullptr, "Unknown2250"}, //20.0.0+
            {2260, nullptr, "Unknown2260"}, //20.0.0+
            {2270, nullptr, "Unknown2270"}, //20.0.0+
            {2280, nullptr, "Unknown2280"}, //20.0.0+
            {2300, nullptr, "Unknown2300"}, //20.0.0+
            {2310, nullptr, "Unknown2310"}, //20.0.0+
            {2400, nullptr, "Unknown2400"}, //20.0.0+
            {2420, nullptr, "Unknown2420"}, //20.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mig:user", std::make_shared<MIG_USR>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Migration
