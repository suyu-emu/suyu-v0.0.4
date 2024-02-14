// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/nvnflinger/hos_binder_driver.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/vi/application_display_service.h"
#include "core/hle/service/vi/application_root_service.h"
#include "core/hle/service/vi/manager_root_service.h"
#include "core/hle/service/vi/system_root_service.h"
#include "core/hle/service/vi/vi.h"

namespace Service::VI {

void LoopProcess(Core::System& system) {
    const auto binder_service =
        system.ServiceManager().GetService<Nvnflinger::IHOSBinderDriver>("dispdrv", true);
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService(
        "vi:m", std::make_shared<IManagerRootService>(system, binder_service));
    server_manager->RegisterNamedService(
        "vi:s", std::make_shared<ISystemRootService>(system, binder_service));
    server_manager->RegisterNamedService(
        "vi:u", std::make_shared<IApplicationRootService>(system, binder_service));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::VI
