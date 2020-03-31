// Copyright 2020 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/caps/caps_su.h"

namespace Service::Capture {

CAPS_SU::CAPS_SU() : ServiceFramework("caps:su") {
    // clang-format off
    static const FunctionInfo functions[] = {
        {201, nullptr, "SaveScreenShot"},
        {203, nullptr, "SaveScreenShotEx0"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

CAPS_SU::~CAPS_SU() = default;

} // namespace Service::Capture
