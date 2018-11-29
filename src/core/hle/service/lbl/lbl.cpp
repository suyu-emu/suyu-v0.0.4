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
            {0, nullptr, "SaveCurrentSetting"},
            {1, nullptr, "LoadCurrentSetting"},
            {2, nullptr, "SetCurrentBrightnessSetting"},
            {3, nullptr, "GetCurrentBrightnessSetting"},
            {4, nullptr, "ApplyCurrentBrightnessSettingToBacklight"},
            {5, nullptr, "GetBrightnessSettingAppliedToBacklight"},
            {6, nullptr, "SwitchBacklightOn"},
            {7, nullptr, "SwitchBacklightOff"},
            {8, nullptr, "GetBacklightSwitchStatus"},
            {9, nullptr, "EnableDimming"},
            {10, nullptr, "DisableDimming"},
            {11, nullptr, "IsDimmingEnabled"},
            {12, nullptr, "EnableAutoBrightnessControl"},
            {13, nullptr, "DisableAutoBrightnessControl"},
            {14, nullptr, "IsAutoBrightnessControlEnabled"},
            {15, nullptr, "SetAmbientLightSensorValue"},
            {16, nullptr, "GetAmbientLightSensorValue"},
            {17, nullptr, "SetBrightnessReflectionDelayLevel"},
            {18, nullptr, "GetBrightnessReflectionDelayLevel"},
            {19, nullptr, "SetCurrentBrightnessMapping"},
            {20, nullptr, "GetCurrentBrightnessMapping"},
            {21, nullptr, "SetCurrentAmbientLightSensorMapping"},
            {22, nullptr, "GetCurrentAmbientLightSensorMapping"},
            {23, nullptr, "IsAmbientLightSensorAvailable"},
            {24, nullptr, "SetCurrentBrightnessSettingForVrMode"},
            {25, nullptr, "GetCurrentBrightnessSettingForVrMode"},
            {26, &LBL::EnableVrMode, "EnableVrMode"},
            {27, &LBL::DisableVrMode, "DisableVrMode"},
            {28, &LBL::IsVrModeEnabled, "IsVrModeEnabled"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void EnableVrMode(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_LBL, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        vr_mode_enabled = true;
    }

    void DisableVrMode(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_LBL, "called");

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        vr_mode_enabled = false;
    }

    void IsVrModeEnabled(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_LBL, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(vr_mode_enabled);
    }

    bool vr_mode_enabled = false;
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LBL>()->InstallAsService(sm);
}

} // namespace Service::LBL
