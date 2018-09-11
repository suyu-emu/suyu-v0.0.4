// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/vi/vi_u.h"

namespace Service::VI {

VI_U::VI_U(std::shared_ptr<Module> module, std::shared_ptr<NVFlinger::NVFlinger> nv_flinger)
    : Module::Interface(std::move(module), "vi:u", std::move(nv_flinger)) {
    static const FunctionInfo functions[] = {
        {0, &VI_U::GetDisplayService, "GetDisplayService"},
    };
    RegisterHandlers(functions);
}

VI_U::~VI_U() = default;

} // namespace Service::VI
