// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "common/logging/log.h"
#include "core/hle/service/nvdrv/nvmemp.h"

namespace Service::Nvidia {

NVMEMP::NVMEMP() : ServiceFramework("nvmemp") {
    static const FunctionInfo functions[] = {
        {0, &NVMEMP::Cmd0, "Cmd0"},
        {1, &NVMEMP::Cmd1, "Cmd1"},
    };
    RegisterHandlers(functions);
}

NVMEMP::~NVMEMP() = default;

void NVMEMP::Cmd0(Kernel::HLERequestContext& ctx) {
    UNIMPLEMENTED();
}

void NVMEMP::Cmd1(Kernel::HLERequestContext& ctx) {
    UNIMPLEMENTED();
}

} // namespace Service::Nvidia
