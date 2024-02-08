// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/bcat/backend/backend.h"
#include "core/hle/service/bcat/bcat.h"
#include "core/hle/service/bcat/bcat_interface.h"
#include "core/hle/service/server_manager.h"

namespace Service::BCAT {

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("bcat:a",
                                         std::make_shared<BcatInterface>(system, "bcat:a"));
    server_manager->RegisterNamedService("bcat:m",
                                         std::make_shared<BcatInterface>(system, "bcat:m"));
    server_manager->RegisterNamedService("bcat:u",
                                         std::make_shared<BcatInterface>(system, "bcat:u"));
    server_manager->RegisterNamedService("bcat:s",
                                         std::make_shared<BcatInterface>(system, "bcat:s"));

    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::BCAT
