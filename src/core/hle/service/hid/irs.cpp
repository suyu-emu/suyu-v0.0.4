// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/hid/irs.h"

namespace Service::HID {

IRS::IRS() : ServiceFramework{"irs"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {302, nullptr, "ActivateIrsensor"},
        {303, nullptr, "DeactivateIrsensor"},
        {304, nullptr, "GetIrsensorSharedMemoryHandle"},
        {305, nullptr, "StopImageProcessor"},
        {306, nullptr, "RunMomentProcessor"},
        {307, nullptr, "RunClusteringProcessor"},
        {308, nullptr, "RunImageTransferProcessor"},
        {309, nullptr, "GetImageTransferProcessorState"},
        {310, nullptr, "RunTeraPluginProcessor"},
        {311, nullptr, "GetNpadIrCameraHandle"},
        {312, nullptr, "RunPointingProcessor"},
        {313, nullptr, "SuspendImageProcessor"},
        {314, nullptr, "CheckFirmwareVersion"},
        {315, nullptr, "SetFunctionLevel"},
        {316, nullptr, "RunImageTransferExProcessor"},
        {317, nullptr, "RunIrLedProcessor"},
        {318, nullptr, "StopImageProcessorAsync"},
        {319, nullptr, "ActivateIrsensorWithFunctionLevel"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IRS::~IRS() = default;

IRS_SYS::IRS_SYS() : ServiceFramework{"irs:sys"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {500, nullptr, "SetAppletResourceUserId"},
        {501, nullptr, "RegisterAppletResourceUserId"},
        {502, nullptr, "UnregisterAppletResourceUserId"},
        {503, nullptr, "EnableAppletToGetInput"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IRS_SYS::~IRS_SYS() = default;

} // namespace Service::HID
