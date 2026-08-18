// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/service/grc/grc.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::GRC {

class GRC final : public ServiceFramework<GRC> {
public:
    explicit GRC(Core::System& system_) : ServiceFramework{system_, "grc:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "OpenContinuousRecorder"},
            {2, nullptr, "OpenGameMovieTrimmer"},
            {3, nullptr, "OpenOffscreenRecorder"},
            {101, nullptr, "CreateMovieMaker"},
            {9903, nullptr, "SetOffscreenRecordingMarker"}
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class GRC_D final : public ServiceFramework<GRC_D> {
public:
    explicit GRC_D(Core::System& system_) : ServiceFramework{system_, "grc:d"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "Initialize"},
            {2, nullptr, "Transfer"},
            {3, nullptr, "Cmd3"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("grc:c", std::make_shared<GRC>(system));
    server_manager->RegisterNamedService("grc:d", std::make_shared<GRC_D>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::GRC
