// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/vi/system_root_service.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

ISystemRootService::ISystemRootService(Core::System& system_, Nvnflinger::Nvnflinger& nv_flinger_,
                                       Nvnflinger::HosBinderDriverServer& hos_binder_driver_server_)
    : ServiceFramework{system_, "vi:s"}, nv_flinger{nv_flinger_},
      hos_binder_driver_server{hos_binder_driver_server_} {
    static const FunctionInfo functions[] = {
        {1, &ISystemRootService::GetDisplayService, "GetDisplayService"},
        {3, nullptr, "GetDisplayServiceWithProxyNameExchange"},
    };
    RegisterHandlers(functions);
}

ISystemRootService::~ISystemRootService() = default;

void ISystemRootService::GetDisplayService(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    detail::GetDisplayServiceImpl(ctx, system, nv_flinger, hos_binder_driver_server,
                                  Permission::System);
}

} // namespace Service::VI
