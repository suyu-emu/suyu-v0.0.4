// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/vi/application_display_service.h"
#include "core/hle/service/vi/application_root_service.h"
#include "core/hle/service/vi/service_creator.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_types.h"

namespace Service::VI {

IApplicationRootService::IApplicationRootService(
    Core::System& system_, std::shared_ptr<Nvnflinger::IHOSBinderDriver> binder_service)
    : ServiceFramework{system_, "vi:u"}, m_binder_service{std::move(binder_service)} {
    static const FunctionInfo functions[] = {
        {0, C<&IApplicationRootService::GetDisplayService>, "GetDisplayService"},
        {1, nullptr, "GetDisplayServiceWithProxyNameExchange"},
    };
    RegisterHandlers(functions);
}

IApplicationRootService::~IApplicationRootService() = default;

Result IApplicationRootService::GetDisplayService(
    Out<SharedPointer<IApplicationDisplayService>> out_application_display_service, Policy policy) {
    LOG_DEBUG(Service_VI, "called");
    R_RETURN(GetApplicationDisplayService(out_application_display_service, system, m_binder_service,
                                          Permission::User, policy));
}

} // namespace Service::VI
