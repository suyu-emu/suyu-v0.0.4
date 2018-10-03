// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
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
            {3, nullptr, "GetCurrentBacklightLevel"},
            {4, nullptr, "Unknown4"},
            {5, nullptr, "GetAlsComputedBacklightLevel"},
            {6, nullptr, "TurnOffBacklight"},
            {7, nullptr, "TurnOnBacklight"},
            {8, nullptr, "GetBacklightStatus"},
            {9, nullptr, "Unknown5"},
            {10, nullptr, "Unknown6"},
            {11, nullptr, "Unknown7"},
            {12, nullptr, "Unknown8"},
            {13, nullptr, "Unknown9"},
            {14, nullptr, "Unknown10"},
            {15, nullptr, "GetAutoBrightnessSetting"},
            {16, nullptr, "ReadRawLightSensor"},
            {17, nullptr, "Unknown11"},
            {18, nullptr, "Unknown12"},
            {19, nullptr, "Unknown13"},
            {20, nullptr, "Unknown14"},
            {21, nullptr, "Unknown15"},
            {22, nullptr, "Unknown16"},
            {23, nullptr, "Unknown17"},
            {24, nullptr, "Unknown18"},
            {25, nullptr, "Unknown19"},
            {26, &LBL::EnableVrMode, "EnableVrMode"},
            {27, &LBL::DisableVrMode, "DisableVrMode"},
            {28, &LBL::GetVrMode, "GetVrMode"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void EnableVrMode(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        vr_mode_enabled = true;

        LOG_DEBUG(Service_LBL, "called");
    }

    void DisableVrMode(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        vr_mode_enabled = false;

        LOG_DEBUG(Service_LBL, "called");
    }

    void GetVrMode(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(vr_mode_enabled);

        LOG_DEBUG(Service_LBL, "called");
    }

    bool vr_mode_enabled = false;
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LBL>()->InstallAsService(sm);
}

} // namespace Service::LBL
