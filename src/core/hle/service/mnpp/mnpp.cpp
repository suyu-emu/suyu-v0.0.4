// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/mnpp/mnpp.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::MNPP {

class MNPP_APP final : public ServiceFramework<MNPP_APP> {
public:
    explicit MNPP_APP(Core::System& system_) : ServiceFramework{system_, "mnpp:app"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &MNPP_APP::Cmd0, "Cmd0"},
            {1, &MNPP_APP::Cmd1, "Cmd1"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Cmd0(HLERequestContext& ctx) {
        LOG_WARNING(Service_MNPP, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void Cmd1(HLERequestContext& ctx) {
        LOG_WARNING(Service_MNPP, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }
};

class MNPP_SYS final : public ServiceFramework<MNPP_SYS> {
public:
    explicit MNPP_SYS(Core::System& system_) : ServiceFramework{system_, "mnpp:sys"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Cmd0"},
            {10, nullptr, "Cmd10"},
            {100, nullptr, "Cmd100"},
            {200, nullptr, "Cmd200"},
            {300, nullptr, "Cmd300"},
            {400, nullptr, "Cmd400"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

class MNPP_WEB final : public ServiceFramework<MNPP_WEB> {
public:
    explicit MNPP_WEB(Core::System& system_) : ServiceFramework{system_, "mnpp:web"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Cmd0"},
            {1, nullptr, "Cmd1"},
            {10, nullptr, "Cmd10"},
            {20, nullptr, "Cmd20"},
            {100, nullptr, "Cmd100"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("mnpp:app", std::make_shared<MNPP_APP>(system));
    server_manager->RegisterNamedService("mnpp:sys", std::make_shared<MNPP_SYS>(system));
    server_manager->RegisterNamedService("mnpp:web", std::make_shared<MNPP_WEB>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::MNPP
