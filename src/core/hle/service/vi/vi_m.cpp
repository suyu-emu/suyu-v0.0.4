// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/vi/vi_m.h"

namespace Service::VI {

VI_M::VI_M(std::shared_ptr<Module> module, std::shared_ptr<NVFlinger::NVFlinger> nv_flinger)
    : Module::Interface(std::move(module), "vi:m", std::move(nv_flinger)) {
    static const FunctionInfo functions[] = {
        {2, &VI_M::GetDisplayService, "GetDisplayService"},
        {3, nullptr, "GetDisplayServiceWithProxyNameExchange"},
    };
    RegisterHandlers(functions);
}

VI_M::~VI_M() = default;

} // namespace Service::VI
