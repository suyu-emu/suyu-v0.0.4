// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ptm/psm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::PSM {

constexpr u32 BATTERY_FULLY_CHARGED = 100;    // 100% Full
constexpr u32 BATTERY_CURRENTLY_CHARGING = 1; // Plugged into an official dock

class PSM final : public ServiceFramework<PSM> {
public:
    explicit PSM() : ServiceFramework{"psm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &PSM::GetBatteryChargePercentage, "GetBatteryChargePercentage"},
            {1, &PSM::GetChargerType, "GetChargerType"},
            {2, nullptr, "EnableBatteryCharging"},
            {3, nullptr, "DisableBatteryCharging"},
            {4, nullptr, "IsBatteryChargingEnabled"},
            {5, nullptr, "AcquireControllerPowerSupply"},
            {6, nullptr, "ReleaseControllerPowerSupply"},
            {7, nullptr, "OpenSession"},
            {8, nullptr, "EnableEnoughPowerChargeEmulation"},
            {9, nullptr, "DisableEnoughPowerChargeEmulation"},
            {10, nullptr, "EnableFastBatteryCharging"},
            {11, nullptr, "DisableFastBatteryCharging"},
            {12, nullptr, "GetBatteryVoltageState"},
            {13, nullptr, "GetRawBatteryChargePercentage"},
            {14, nullptr, "IsEnoughPowerSupplied"},
            {15, nullptr, "GetBatteryAgePercentage"},
            {16, nullptr, "GetBatteryChargeInfoEvent"},
            {17, nullptr, "GetBatteryChargeInfoFields"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

    ~PSM() override = default;

private:
    void GetBatteryChargePercentage(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_PSM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(BATTERY_FULLY_CHARGED);
    }

    void GetChargerType(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_PSM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(BATTERY_CURRENTLY_CHARGING);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<PSM>()->InstallAsService(sm);
}

} // namespace Service::PSM
