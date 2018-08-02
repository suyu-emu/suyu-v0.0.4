// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/bpc/bpc.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::BPC {

class BPC final : public ServiceFramework<BPC> {
public:
    explicit BPC() : ServiceFramework{"bpc"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "ShutdownSystem"},
            {1, nullptr, "RebootSystem"},
            {2, nullptr, "GetWakeupReason"},
            {3, nullptr, "GetShutdownReason"},
            {4, nullptr, "GetAcOk"},
            {5, nullptr, "GetBoardPowerControlEvent"},
            {6, nullptr, "GetSleepButtonState"},
            {7, nullptr, "GetPowerEvent"},
            {8, nullptr, "Unknown1"},
            {9, nullptr, "Unknown2"},
            {10, nullptr, "Unknown3"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class BPC_R final : public ServiceFramework<BPC_R> {
public:
    explicit BPC_R() : ServiceFramework{"bpc:r"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetExternalRtcValue"},
            {1, nullptr, "SetExternalRtcValue"},
            {2, nullptr, "ReadExternalRtcResetFlag"},
            {3, nullptr, "ClearExternalRtcResetFlag"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<BPC>()->InstallAsService(sm);
    std::make_shared<BPC_R>()->InstallAsService(sm);
}

} // namespace Service::BPC
