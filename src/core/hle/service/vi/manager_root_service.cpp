// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/vi/application_display_service.h"
#include "core/hle/service/vi/manager_root_service.h"
#include "core/hle/service/vi/service_creator.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

IManagerRootService::IManagerRootService(
    Core::System& system_, Nvnflinger::Nvnflinger& nvnflinger,
    Nvnflinger::HosBinderDriverServer& hos_binder_driver_server)
    : ServiceFramework{system_, "vi:m"}, m_nvnflinger{nvnflinger}, m_hos_binder_driver_server{
                                                                       hos_binder_driver_server} {
    static const FunctionInfo functions[] = {
        {2, C<&IManagerRootService::GetDisplayService>, "GetDisplayService"},
        {3, nullptr, "GetDisplayServiceWithProxyNameExchange"},
        {100, nullptr, "PrepareFatal"},
        {101, nullptr, "ShowFatal"},
        {102, nullptr, "DrawFatalRectangle"},
        {103, nullptr, "DrawFatalText32"},
    };
    RegisterHandlers(functions);
}

IManagerRootService::~IManagerRootService() = default;

Result IManagerRootService::GetDisplayService(
    Out<SharedPointer<IApplicationDisplayService>> out_application_display_service, Policy policy) {
    LOG_DEBUG(Service_VI, "called");
    R_RETURN(GetApplicationDisplayService(out_application_display_service, system, m_nvnflinger,
                                          m_hos_binder_driver_server, Permission::Manager, policy));
}

} // namespace Service::VI
