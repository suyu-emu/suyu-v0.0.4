// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/vi/vi_s.h"

namespace Service::VI {

VI_S::VI_S(std::shared_ptr<Module> module, std::shared_ptr<NVFlinger::NVFlinger> nv_flinger)
    : Module::Interface(std::move(module), "vi:s", std::move(nv_flinger)) {
    static const FunctionInfo functions[] = {
        {1, &VI_S::GetDisplayService, "GetDisplayService"},
        {3, nullptr, "GetDisplayServiceWithProxyNameExchange"},
    };
    RegisterHandlers(functions);
}

VI_S::~VI_S() = default;

} // namespace Service::VI
