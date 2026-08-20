// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/service/bpc/bpc.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::BPC {

class BPC final : public ServiceFramework<BPC> {
public:
    explicit BPC(Core::System& system_) : ServiceFramework{system_, "bpc"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "ShutdownSystem"},
            {1, nullptr, "RebootSystem"},
            {2, nullptr, "GetWakeupReason"},
            {3, nullptr, "GetShutdownReason"},
            {4, nullptr, "GetAcOk"},
            {5, nullptr, "GetBoardPowerControlEvent"},
            {6, nullptr, "GetSleepButtonState"},
            {7, nullptr, "GetPowerEvent"},
            {8, nullptr, "CreateWakeupTimer"},
            {9, nullptr, "CancelWakeupTimer"},
            {10, nullptr, "EnableWakeupTimerOnDevice"},
            {11, nullptr, "CreateWakeupTimerEx"},
            {12, nullptr, "GetLastEnabledWakeupTimerType"},
            {13, nullptr, "CleanAllWakeupTimers"},
            {14, nullptr, "GetPowerButton"},
            {15, nullptr, "SetEnableWakeupTimer"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class BPC_R final : public ServiceFramework<BPC_R> {
public:
    explicit BPC_R(Core::System& system_) : ServiceFramework{system_, "bpc:r"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetRtcTime"},
            {1, nullptr, "SetRtcTime"},
            {2, nullptr, "GetRtcResetDetected"},
            {3, nullptr, "ClearRtcResetDetected"},
            {4, nullptr, "SetUpRtcResetOnShutdown"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class BPC_C final : public ServiceFramework<BPC_C> {
public:
    explicit BPC_C(Core::System& system_) : ServiceFramework{system_, "bpc:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "ShutdownSystem"},
            {1, nullptr, "RebootSystem"},
            {2, nullptr, "GetWakeupReason"},
            {3, nullptr, "GetShutdownReason"},
            {4, nullptr, "GetAcOk"},
            {5, nullptr, "GetPowerEvent"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class BPC_B final : public ServiceFramework<BPC_B> {
public:
    explicit BPC_B(Core::System& system_) : ServiceFramework{system_, "bpc:b"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetSleepButtonState"},
            {1, nullptr, "GetPowerButtonEvent"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class BPC_W final : public ServiceFramework<BPC_W> {
public:
    explicit BPC_W(Core::System& system_) : ServiceFramework{system_, "bpc:w"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateWakeupTimer"},
            {1, nullptr, "CancelWakeupTimer"},
            {2, nullptr, "EnableWakeupTimerOnDevice"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("bpc", std::make_shared<BPC>(system));
    server_manager->RegisterNamedService("bpc:r", std::make_shared<BPC_R>(system));
    server_manager->RegisterNamedService("bpc:c", std::make_shared<BPC_C>(system));
    server_manager->RegisterNamedService("bpc:b", std::make_shared<BPC_B>(system));
    server_manager->RegisterNamedService("bpc:w", std::make_shared<BPC_W>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::BPC
