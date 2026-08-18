// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/service/pcie/pcie.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"
#include "frontend_common/firmware_manager.h"

namespace Service::PCIe {

class ISession final : public ServiceFramework<ISession> {
public:
    explicit ISession(Core::System& system_) : ServiceFramework{system_, "ISession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "QueryFunctions"},
            {1, nullptr, "AcquireFunction"},
            {2, nullptr, "ReleaseFunction"},
            {3, nullptr, "GetFunctionState"},
            {4, nullptr, "GetBarProfile"},
            {5, nullptr, "ReadConfig"},
            {6, nullptr, "WriteConfig"},
            {7, nullptr, "ReadBarRegion"},
            {8, nullptr, "WriteBarRegion"},
            {9, nullptr, "FindCapability"},
            {10, nullptr, "FindExtendedCapability"},
            {11, nullptr, "MapDma"},
            {12, nullptr, "UnmapDma"},
            {13, nullptr, "UnmapDmaBusAddress"},
            {14, nullptr, "GetDmaBusAddress"},
            {15, nullptr, "GetDmaBusAddressRange"},
            {16, nullptr, "SetDmaEnable"},
            {17, nullptr, "AcquireIrq"},
            {18, nullptr, "ReleaseIrq"},
            {19, nullptr, "SetIrqEnable"},
            {20, nullptr, "GetIrqEvent"},
            {21, nullptr, "SetAspmEnable"},
            {22, nullptr, "SetResetUponResumeEnable"},
            {23, nullptr, "ResetFunction"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class PCIE final : public ServiceFramework<PCIE> {
public:
    explicit PCIE(Core::System& system_) : ServiceFramework{system_, "pcie"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RegisterClassDriver"},
            {1, nullptr, "QueryFunctionsUnregistered"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class PCIE_LOG final : public ServiceFramework<PCIE_LOG> {
public:
    explicit PCIE_LOG(Core::System& system_) : ServiceFramework{system_, "pcie:log"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetLoggedState"},
            {1, nullptr, "GetLoggedStateEvent"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("pcie", std::make_shared<PCIE>(system));
    // +6.0.0
    if (FirmwareManager::GetFirmwareVersion(system).first.major >= 6) {
        server_manager->RegisterNamedService("pcie:log", std::make_shared<PCIE_LOG>(system));
    }
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::PCIe
