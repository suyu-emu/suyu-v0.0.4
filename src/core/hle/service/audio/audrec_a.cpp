// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/audio/audrec_a.h"

namespace Service::Audio {

AudRecA::AudRecA() : ServiceFramework{"audrec:a"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestSuspendFinalOutputRecorders"},
        {1, nullptr, "RequestResumeFinalOutputRecorders"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

} // namespace Service::Audio
