// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/server_manager.h"
#include "core/hle/service/sockets/bsd.h"
#include "core/hle/service/sockets/nsd.h"
#include "core/hle/service/sockets/sfdnsres.h"
#include "core/hle/service/sockets/sockets.h"

namespace Service::Sockets {

class ETHC_C final : public ServiceFramework<ETHC_C> {
public:
    explicit ETHC_C(Core::System& system_)
        : ServiceFramework{system_, "ethc:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Cancel"},
            {2, nullptr, "GetResult"},
            {3, nullptr, "GetMediaList"},
            {4, nullptr, "SetMediaType"},
            {5, nullptr, "GetMediaType"},
            {6, nullptr, "GetMacAddress"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class ETHC_I final : public ServiceFramework<ETHC_I> {
public:
    explicit ETHC_I(Core::System& system_)
        : ServiceFramework{system_, "ethc:i"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetReadableHandle"},
            {1, nullptr, "Cancel"},
            {2, nullptr, "GetResult"},
            {3, nullptr, "GetInterfaceList"},
            {4, nullptr, "GetInterfaceCount"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class ISfDriverServiceCreator final : public ServiceFramework<ISfDriverServiceCreator> {
public:
    explicit ISfDriverServiceCreator(Core::System& system_)
        : ServiceFramework{system_, "eth:nd"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateDriverService"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("ethc:c", std::make_shared<ETHC_C>(system));
    server_manager->RegisterNamedService("ethc:i", std::make_shared<ETHC_I>(system));
    server_manager->RegisterNamedService("bsd:s", std::make_shared<BSD>(system, "bsd:s", false));
    server_manager->RegisterNamedService("bsd:u", std::make_shared<BSD>(system, "bsd:u", true));
    server_manager->RegisterNamedService("bsd:a", std::make_shared<BSD>(system, "bsd:a", true));
    server_manager->RegisterNamedService("bsd:nu", std::make_shared<BSD_NU>(system));
    server_manager->RegisterNamedService("bsdcfg", std::make_shared<BSDCFG>(system, "bsdcfg"));
    server_manager->RegisterNamedService("ifcfg", std::make_shared<BSDCFG>(system, "ifcfg"));
    server_manager->RegisterNamedService("nsd:a", std::make_shared<NSD>(system, "nsd:a"));
    server_manager->RegisterNamedService("nsd:u", std::make_shared<NSD>(system, "nsd:u"));
    server_manager->RegisterNamedService("sfdnsres", std::make_shared<SFDNSRES>(system));
    server_manager->RegisterNamedService("dns:priv", std::make_shared<DNS_PRIV>(system));
    server_manager->RegisterNamedService("eth:nd", std::make_shared<ISfDriverServiceCreator>(system));

    server_manager->StartAdditionalHostThreads("bsdsocket", 2);
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::Sockets
