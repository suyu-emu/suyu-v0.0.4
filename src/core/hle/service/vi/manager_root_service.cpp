// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/vi/manager_root_service.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

IManagerRootService::IManagerRootService(
    Core::System& system_, Nvnflinger::Nvnflinger& nv_flinger_,
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server_)
    : ServiceFramework{system_, "vi:m"}, nv_flinger{nv_flinger_},
      hos_binder_driver_server{hos_binder_driver_server_} {
    static const FunctionInfo functions[] = {
        {2, &IManagerRootService::GetDisplayService, "GetDisplayService"},
        {3, nullptr, "GetDisplayServiceWithProxyNameExchange"},
        {100, nullptr, "PrepareFatal"},
        {101, nullptr, "ShowFatal"},
        {102, nullptr, "DrawFatalRectangle"},
        {103, nullptr, "DrawFatalText32"},
    };
    RegisterHandlers(functions);
}

IManagerRootService::~IManagerRootService() = default;

void IManagerRootService::GetDisplayService(HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    detail::GetDisplayServiceImpl(ctx, system, nv_flinger, hos_binder_driver_server,
                                  Permission::Manager);
}

} // namespace Service::VI
