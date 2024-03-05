// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/am.h"
#include "core/hle/service/am/service/all_system_applet_proxies_service.h"
#include "core/hle/service/am/service/application_proxy_service.h"
#include "core/hle/service/server_manager.h"

namespace Service::AM {

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("appletAE",
                                         std::make_shared<IAllSystemAppletProxiesService>(system));
    server_manager->RegisterNamedService("appletOE",
                                         std::make_shared<IApplicationProxyService>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::AM
