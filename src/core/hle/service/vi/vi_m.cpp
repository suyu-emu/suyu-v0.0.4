// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/service/vi/vi.h"
#include "core/hle/service/vi/vi_m.h"

namespace Service::VI {

VI_M::VI_M(std::shared_ptr<NVFlinger::NVFlinger> nv_flinger)
    : ServiceFramework{"vi:m"}, nv_flinger{std::move(nv_flinger)} {
    static const FunctionInfo functions[] = {
        {2, &VI_M::GetDisplayService, "GetDisplayService"},
        {3, nullptr, "GetDisplayServiceWithProxyNameExchange"},
    };
    RegisterHandlers(functions);
}

VI_M::~VI_M() = default;

void VI_M::GetDisplayService(Kernel::HLERequestContext& ctx) {
    LOG_DEBUG(Service_VI, "called");

    detail::GetDisplayServiceImpl(ctx, nv_flinger, Permission::Manager);
}

} // namespace Service::VI
