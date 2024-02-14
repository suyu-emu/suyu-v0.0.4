// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/vi/application_display_service.h"
#include "core/hle/service/vi/application_root_service.h"
#include "core/hle/service/vi/manager_root_service.h"
#include "core/hle/service/vi/system_root_service.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_results.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

static bool IsValidServiceAccess(Permission permission, Policy policy) {
    if (permission == Permission::User) {
        return policy == Policy::User;
    }

    if (permission == Permission::System || permission == Permission::Manager) {
        return policy == Policy::User || policy == Policy::Compositor;
    }

    return false;
}

void detail::GetDisplayServiceImpl(HLERequestContext& ctx, Core::System& system,
                                   Nvnflinger::Nvnflinger& nvnflinger,
                                   Nvnflinger::HosBinderDriverServer& hos_binder_driver_server,
                                   Permission permission) {
    IPC::RequestParser rp{ctx};
    const auto policy = rp.PopEnum<Policy>();

    if (!IsValidServiceAccess(permission, policy)) {
        LOG_ERROR(Service_VI, "Permission denied for policy {}", policy);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultPermissionDenied);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IApplicationDisplayService>(system, nvnflinger, hos_binder_driver_server);
}

void LoopProcess(Core::System& system, Nvnflinger::Nvnflinger& nvnflinger,
                 Nvnflinger::HosBinderDriverServer& hos_binder_driver_server) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("vi:m", std::make_shared<IManagerRootService>(
                                                     system, nvnflinger, hos_binder_driver_server));
    server_manager->RegisterNamedService(
        "vi:s", std::make_shared<ISystemRootService>(system, nvnflinger, hos_binder_driver_server));
    server_manager->RegisterNamedService("vi:u", std::make_shared<IApplicationRootService>(
                                                     system, nvnflinger, hos_binder_driver_server));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::VI
