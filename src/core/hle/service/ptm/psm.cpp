// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "common/common_funcs.h"
#include "common/device_power_state.h"
#include "common/logging.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/result.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/ptm/psm.h"

namespace Service::PTM {

class IPsmSession final : public ServiceFramework<IPsmSession> {
public:
    explicit IPsmSession(Core::System& system_)
        : ServiceFramework{system_, "IPsmSession"}, service_context{system_, "IPsmSession"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IPsmSession::BindStateChangeEvent, "BindStateChangeEvent"},
            {1, &IPsmSession::UnbindStateChangeEvent, "UnbindStateChangeEvent"},
            {2, &IPsmSession::SetChargerTypeChangeEventEnabled, "SetChargerTypeChangeEventEnabled"},
            {3, &IPsmSession::SetPowerSupplyChangeEventEnabled, "SetPowerSupplyChangeEventEnabled"},
            {4, &IPsmSession::SetBatteryVoltageStateChangeEventEnabled, "SetBatteryVoltageStateChangeEventEnabled"},
        };
        // clang-format on

        RegisterHandlers(functions);

        state_change_event = service_context.CreateEvent("IPsmSession::state_change_event");
    }

    ~IPsmSession() override {
        service_context.CloseEvent(state_change_event);
    }

    void SignalChargerTypeChanged() {
        if (should_signal && should_signal_charger_type) {
            state_change_event->Signal(system.Kernel());
        }
    }

    void SignalPowerSupplyChanged() {
        if (should_signal && should_signal_power_supply) {
            state_change_event->Signal(system.Kernel());
        }
    }

    void SignalBatteryVoltageStateChanged() {
        if (should_signal && should_signal_battery_voltage) {
            state_change_event->Signal(system.Kernel());
        }
    }

private:
    void BindStateChangeEvent(HLERequestContext& ctx) {
        LOG_DEBUG(Service_PTM, "called");

        should_signal = true;

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(ResultSuccess);
        rb.PushCopyObjects(ctx, state_change_event->GetReadableEvent());
    }

    void UnbindStateChangeEvent(HLERequestContext& ctx) {
        LOG_DEBUG(Service_PTM, "called");

        should_signal = false;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetChargerTypeChangeEventEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto state = rp.Pop<bool>();
        LOG_DEBUG(Service_PTM, "called, state={}", state);

        should_signal_charger_type = state;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetPowerSupplyChangeEventEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto state = rp.Pop<bool>();
        LOG_DEBUG(Service_PTM, "called, state={}", state);

        should_signal_power_supply = state;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    void SetBatteryVoltageStateChangeEventEnabled(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto state = rp.Pop<bool>();
        LOG_DEBUG(Service_PTM, "called, state={}", state);

        should_signal_battery_voltage = state;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    KernelHelpers::ServiceContext service_context;

    bool should_signal_charger_type{};
    bool should_signal_power_supply{};
    bool should_signal_battery_voltage{};
    bool should_signal{};
    Kernel::KEvent* state_change_event;
};

PSM::PSM(Core::System& system_) : ServiceFramework{system_, "psm"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &PSM::GetBatteryChargePercentage, "GetBatteryChargePercentage"},
        {1, &PSM::GetChargerType, "GetChargerType"},
        {2, nullptr, "EnableBatteryCharging"},
        {3, nullptr, "DisableBatteryCharging"},
        {4, nullptr, "IsBatteryChargingEnabled"},
        {5, nullptr, "AcquireControllerPowerSupply"},
        {6, nullptr, "ReleaseControllerPowerSupply"},
        {7, &PSM::OpenSession, "OpenSession"},
        {8, nullptr, "EnableEnoughPowerChargeEmulation"},
        {9, nullptr, "DisableEnoughPowerChargeEmulation"},
        {10, nullptr, "EnableFastBatteryCharging"},
        {11, nullptr, "DisableFastBatteryCharging"},
        {12, &PSM::GetBatteryVoltageState, "GetBatteryVoltageState"},
        {13, nullptr, "GetRawBatteryChargePercentage"},
        {14, nullptr, "IsEnoughPowerSupplied"},
        {15, &PSM::GetBatteryAgePercentage, "GetBatteryAgePercentage"},
        {16, nullptr, "GetBatteryChargeInfoEvent"},
        {17, &PSM::GetBatteryChargeInfoFields, "GetBatteryChargeInfoFields"},
        {18, nullptr, "GetBatteryChargeCalibratedEvent"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

PSM::~PSM() = default;

void PSM::GetBatteryChargePercentage(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");

    u32 percentage = 100;

    Common::PowerStatus power_status = Common::GetPowerStatus();

    if (power_status.has_battery && power_status.percentage >= 0) {
        percentage = static_cast<u32>(power_status.percentage);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push<u32>(percentage);
}

void PSM::GetChargerType(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");

    ChargerType charger = ChargerType::Unplugged;
    Common::PowerStatus power_status = Common::GetPowerStatus();
    if (power_status.has_battery && power_status.charging) {
        charger = ChargerType::RegularCharger;
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(charger);
}

void PSM::OpenSession(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IPsmSession>(ctx, system);
}

void PSM::GetBatteryVoltageState(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "(stubbed)");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<u32>(0); //
}

void PSM::GetBatteryAgePercentage(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "(stubbed)");
    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<f64>(1.f);
}

struct BatteryChargeInfoFields {
    u32 input_current_limit; //mA
    u32 boost_mode_current_limit;
    u32 fast_charge_current_limit;
    u32 charge_voltage_limit;
    u32 charger_type;
    u8 hi2_mode;
    u8 battery_charging;
    INSERT_PADDING_BYTES_NOINIT(2);
    u32 vdd50_state;
    u32 temperature_celcius;
    f32 battery_charge_percentage;
    u32 battery_charge_milli_voltage;
    f32 battery_age_percentage;
    u32 usb_power_role;
    u32 usb_charger_type;
    u32 charger_input_voltage_limit;
    u32 charger_input_current_limit;
    u8 fast_battery_charging;
    u8 controller_power_supply;
    u8 otg_request;
    INSERT_PADDING_BYTES_NOINIT(1);
    INSERT_PADDING_BYTES_NOINIT(0x14); //[+17.0.0]
};
static_assert(sizeof(struct BatteryChargeInfoFields) == 0x54);
void PSM::GetBatteryChargeInfoFields(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PTM, "called");
    Common::PowerStatus power_status = Common::GetPowerStatus();

    BatteryChargeInfoFields r{};
    r.battery_charge_percentage = f32(power_status.percentage); //100%
    r.battery_age_percentage = f32(power_status.percentage); //100%
    r.battery_charging = power_status.charging ? 1 : 0;
    r.charger_type = u32(power_status.has_battery && power_status.charging
        ? ChargerType::RegularCharger : ChargerType::Unplugged);
    r.charger_input_voltage_limit = 100;
    r.charger_input_voltage_limit = 100;
    r.input_current_limit = 100;
    r.boost_mode_current_limit = 100;
    r.fast_charge_current_limit = 100;

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushRaw<BatteryChargeInfoFields>(r);
}

} // namespace Service::PTM
