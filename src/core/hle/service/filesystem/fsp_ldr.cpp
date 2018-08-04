// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/filesystem/fsp_ldr.h"
#include "core/hle/service/service.h"

namespace Service::FileSystem {

FSP_LDR::FSP_LDR() : ServiceFramework{"fsp:ldr"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "OpenCodeFileSystem"},
        {1, nullptr, "IsArchivedProgram"},
        {2, nullptr, "SetCurrentProcess"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

} // namespace Service::FileSystem
