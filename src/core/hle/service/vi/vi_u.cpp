// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_u.h"

namespace Service::VI {

VI_U::VI_U(std::shared_ptr<NVFlinger::NVFlinger> nv_flinger)
    : ServiceFramework{"vi:u"}, nv_flinger{std::move(nv_flinger)} {
    static const FunctionInfo functions[] = {
        {0, &VI_U::GetDisplayService, "GetDisplayService"},
    };
    RegisterHandlers(functions);
}

VI_U::~VI_U() = default;

void VI_U::GetDisplayService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    detail::GetDisplayServiceImpl(ctx, nv_flinger, Permission::User);
}

} // namespace Service::VI
