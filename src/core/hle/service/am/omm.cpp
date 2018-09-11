// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/am/omm.h"

namespace Service::AM {

OMM::OMM() : ServiceFramework{"omm"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "GetOperationMode"},
        {1, nullptr, "GetOperationModeChangeEvent"},
        {2, nullptr, "EnableAudioVisual"},
        {3, nullptr, "DisableAudioVisual"},
        {4, nullptr, "EnterSleepAndWait"},
        {5, nullptr, "GetCradleStatus"},
        {6, nullptr, "FadeInDisplay"},
        {7, nullptr, "FadeOutDisplay"},
        {8, nullptr, "Unknown1"},
        {9, nullptr, "Unknown2"},
        {10, nullptr, "Unknown3"},
        {11, nullptr, "Unknown4"},
        {12, nullptr, "Unknown5"},
        {13, nullptr, "Unknown6"},
        {14, nullptr, "Unknown7"},
        {15, nullptr, "Unknown8"},
        {16, nullptr, "Unknown9"},
        {17, nullptr, "Unknown10"},
        {18, nullptr, "Unknown11"},
        {19, nullptr, "Unknown12"},
        {20, nullptr, "Unknown13"},
        {21, nullptr, "Unknown14"},
        {22, nullptr, "Unknown15"},
        {23, nullptr, "Unknown16"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

OMM::~OMM() = default;

} // namespace Service::AM
