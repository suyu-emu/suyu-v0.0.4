// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/lbl/lbl.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::LBL {

class LBL final : public ServiceFramework<LBL> {
public:
    explicit LBL() : ServiceFramework{"lbl"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "Unknown6"},
            {6, nullptr, "TurnOffBacklight"},
            {7, nullptr, "TurnOnBacklight"},
            {8, nullptr, "GetBacklightStatus"},
            {9, nullptr, "Unknown7"},
            {10, nullptr, "Unknown8"},
            {11, nullptr, "Unknown9"},
            {12, nullptr, "Unknown10"},
            {13, nullptr, "Unknown11"},
            {14, nullptr, "Unknown12"},
            {15, nullptr, "Unknown13"},
            {16, nullptr, "ReadRawLightSensor"},
            {17, nullptr, "Unknown14"},
            {18, nullptr, "Unknown15"},
            {19, nullptr, "Unknown16"},
            {20, nullptr, "Unknown17"},
            {21, nullptr, "Unknown18"},
            {22, nullptr, "Unknown19"},
            {23, nullptr, "Unknown20"},
            {24, nullptr, "Unknown21"},
            {25, nullptr, "Unknown22"},
            {26, nullptr, "EnableVrMode"},
            {27, nullptr, "DisableVrMode"},
            {28, nullptr, "GetVrMode"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LBL>()->InstallAsService(sm);
}

} // namespace Service::LBL
