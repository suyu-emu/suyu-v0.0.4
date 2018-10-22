// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/am/idle.h"

namespace Service::AM {

IdleSys::IdleSys() : ServiceFramework{"idle:sys"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetAutoPowerDownEvent"},
        {1, nullptr, "Unknown1"},
        {2, nullptr, "Unknown2"},
        {3, nullptr, "SetHandlingContext"},
        {4, nullptr, "LoadAndApplySettings"},
        {5, nullptr, "ReportUserIsActive"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IdleSys::~IdleSys() = default;

} // namespace Service::AM
