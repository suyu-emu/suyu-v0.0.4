// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/audio/auddbg.h"

namespace Service::Audio {

AudDbg::AudDbg(const char* name) : ServiceFramework{name} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspendForDebug"},
        {1, nullptr, "RequestResumeForDebug"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

} // namespace Service::Audio
