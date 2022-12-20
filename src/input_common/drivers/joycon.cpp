// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <fmt/format.h>

#include "common/param_package.h"
#include "common/settings.h"
#include "common/thread.h"
#include "input_common/drivers/joycon.h"
#include "input_common/helpers/joycon_driver.h"
#include "input_common/helpers/joycon_protocol/joycon_types.h"

namespace InputCommon {

Joycons::Joycons(const std::string& input_engine_) : InputEngine(input_engine_) {
    // Avoid conflicting with SDL driver
    if (!Settings::values.enable_joycon_driver) {
        return;
    }
    LOG_INFO(Input, "Joycon driver Initialization started");
    const int init_res = SDL_hid_init();
    if (init_res == 0) {
        Setup();
    } else {
        LOG_ERROR(Input, "Hidapi could not be initialized. failed with error = {}", init_res);
    }
}

Joycons::~Joycons() {
    Reset();
}

void Joycons::Reset() {
    scan_thread = {};
    for (const auto& device : left_joycons) {
        if (!device) {
            continue;
        }
        device->Stop();
    }
    for (const auto& device : right_joycons) {
        if (!device) {
            continue;
        }
        device->Stop();
    }
    for (const auto& device : pro_joycons) {
        if (!device) {
            continue;
        }
        device->Stop();
    }
    SDL_hid_exit();
}

void Joycons::Setup() {
    u32 port = 0;
    for (auto& device : left_joycons) {
        PreSetController(GetIdentifier(port, Joycon::ControllerType::Left));
        device = std::make_shared<Joycon::JoyconDriver>(port++);
    }
    for (auto& device : right_joycons) {
        PreSetController(GetIdentifier(port, Joycon::ControllerType::Right));
        device = std::make_shared<Joycon::JoyconDriver>(port++);
    }
    for (auto& device : pro_joycons) {
        PreSetController(GetIdentifier(port, Joycon::ControllerType::Pro));
        device = std::make_shared<Joycon::JoyconDriver>(port++);
    }

    if (!scan_thread_running) {
        scan_thread = std::jthread([this](std::stop_token stop_token) { ScanThread(stop_token); });
    }
}

void Joycons::ScanThread(std::stop_token stop_token) {
    constexpr u16 nintendo_vendor_id = 0x057e;
    Common::SetCurrentThreadName("yuzu:input:JoyconScanThread");
    scan_thread_running = true;
    while (!stop_token.stop_requested()) {
        SDL_hid_device_info* devs = SDL_hid_enumerate(nintendo_vendor_id, 0x0);
        SDL_hid_device_info* cur_dev = devs;

        while (cur_dev) {
            if (IsDeviceNew(cur_dev)) {
                LOG_DEBUG(Input, "Device Found,type : {:04X} {:04X}", cur_dev->vendor_id,
                          cur_dev->product_id);
                RegisterNewDevice(cur_dev);
            }
            cur_dev = cur_dev->next;
        }

        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    scan_thread_running = false;
}

bool Joycons::IsDeviceNew(SDL_hid_device_info* device_info) const {
    Joycon::ControllerType type{};
    Joycon::SerialNumber serial_number{};

    const auto result = Joycon::JoyconDriver::GetDeviceType(device_info, type);
    if (result != Joycon::DriverResult::Success) {
        return false;
    }

    const auto result2 = Joycon::JoyconDriver::GetSerialNumber(device_info, serial_number);
    if (result2 != Joycon::DriverResult::Success) {
        return false;
    }

    auto is_handle_identical = [&](std::shared_ptr<Joycon::JoyconDriver> device) {
        if (!device) {
            return false;
        }
        if (!device->IsConnected()) {
            return false;
        }
        if (device->GetHandleSerialNumber() != serial_number) {
            return false;
        }
        return true;
    };

    // Check if device already exist
    switch (type) {
    case Joycon::ControllerType::Left:
        for (const auto& device : left_joycons) {
            if (is_handle_identical(device)) {
                return false;
            }
        }
        break;
    case Joycon::ControllerType::Right:
        for (const auto& device : right_joycons) {
            if (is_handle_identical(device)) {
                return false;
            }
        }
        break;
    case Joycon::ControllerType::Pro:
    case Joycon::ControllerType::Grip:
        for (const auto& device : pro_joycons) {
            if (is_handle_identical(device)) {
                return false;
            }
        }
        break;
    default:
        return false;
    }

    return true;
}

void Joycons::RegisterNewDevice(SDL_hid_device_info* device_info) {
    Joycon::ControllerType type{};
    auto result = Joycon::JoyconDriver::GetDeviceType(device_info, type);
    auto handle = GetNextFreeHandle(type);
    if (handle == nullptr) {
        LOG_WARNING(Input, "No free handles available");
        return;
    }
    if (result == Joycon::DriverResult::Success) {
        result = handle->RequestDeviceAccess(device_info);
    }
    if (result == Joycon::DriverResult::Success) {
        LOG_WARNING(Input, "Initialize device");

        std::function<void(Joycon::Battery)> on_battery_data;
        std::function<void(Joycon::Color)> on_button_data;
        std::function<void(int, f32)> on_stick_data;
        std::function<void(int, std::array<u8, 6>)> on_motion_data;
        std::function<void(s16)> on_ring_data;
        std::function<void(const std::vector<u8>&)> on_amiibo_data;

        const std::size_t port = handle->GetDevicePort();
        handle->on_battery_data = {
            [this, port, type](Joycon::Battery value) { OnBatteryUpdate(port, type, value); }};
        handle->on_color_data = {
            [this, port, type](Joycon::Color value) { OnColorUpdate(port, type, value); }};
        handle->on_button_data = {
            [this, port, type](int id, bool value) { OnButtonUpdate(port, type, id, value); }};
        handle->on_stick_data = {
            [this, port, type](int id, f32 value) { OnStickUpdate(port, type, id, value); }};
        handle->on_motion_data = {[this, port, type](int id, Joycon::MotionData value) {
            OnMotionUpdate(port, type, id, value);
        }};
        handle->on_ring_data = {[this](f32 ring_data) { OnRingConUpdate(ring_data); }};
        handle->on_amiibo_data = {[this, port](const std::vector<u8>& amiibo_data) {
            OnAmiiboUpdate(port, amiibo_data);
        }};
        handle->InitializeDevice();
    }
}

std::shared_ptr<Joycon::JoyconDriver> Joycons::GetNextFreeHandle(
    Joycon::ControllerType type) const {

    if (type == Joycon::ControllerType::Left) {
        for (const auto& device : left_joycons) {
            if (!device->IsConnected()) {
                return device;
            }
        }
    }
    if (type == Joycon::ControllerType::Right) {
        for (const auto& device : right_joycons) {
            if (!device->IsConnected()) {
                return device;
            }
        }
    }
    if (type == Joycon::ControllerType::Pro || type == Joycon::ControllerType::Grip) {
        for (const auto& device : pro_joycons) {
            if (!device->IsConnected()) {
                return device;
            }
        }
    }
    return nullptr;
}

bool Joycons::IsVibrationEnabled(const PadIdentifier& identifier) {
    const auto handle = GetHandle(identifier);
    if (handle == nullptr) {
        return false;
    }
    return handle->IsVibrationEnabled();
}

Common::Input::VibrationError Joycons::SetVibration(
    const PadIdentifier& identifier, const Common::Input::VibrationStatus& vibration) {
    const Joycon::VibrationValue native_vibration{
        .low_amplitude = vibration.low_amplitude,
        .low_frequency = vibration.low_frequency,
        .high_amplitude = vibration.high_amplitude,
        .high_frequency = vibration.high_amplitude,
    };
    auto handle = GetHandle(identifier);
    if (handle == nullptr) {
        return Common::Input::VibrationError::InvalidHandle;
    }

    handle->SetVibration(native_vibration);
    return Common::Input::VibrationError::None;
}

void Joycons::SetLeds(const PadIdentifier& identifier, const Common::Input::LedStatus& led_status) {
    auto handle = GetHandle(identifier);
    if (handle == nullptr) {
        return;
    }
    int led_config = led_status.led_1 ? 1 : 0;
    led_config += led_status.led_2 ? 2 : 0;
    led_config += led_status.led_3 ? 4 : 0;
    led_config += led_status.led_4 ? 8 : 0;

    const auto result = handle->SetLedConfig(static_cast<u8>(led_config));
    if (result != Joycon::DriverResult::Success) {
        LOG_ERROR(Input, "Failed to set led config");
    }
}

Common::Input::CameraError Joycons::SetCameraFormat(const PadIdentifier& identifier_,
                                                    Common::Input::CameraFormat camera_format) {
    return Common::Input::CameraError::NotSupported;
};

Common::Input::NfcState Joycons::SupportsNfc(const PadIdentifier& identifier_) const {
    return Common::Input::NfcState::Success;
};

Common::Input::NfcState Joycons::WriteNfcData(const PadIdentifier& identifier_,
                                              const std::vector<u8>& data) {
    return Common::Input::NfcState::NotSupported;
};

Common::Input::PollingError Joycons::SetPollingMode(const PadIdentifier& identifier,
                                                    const Common::Input::PollingMode polling_mode) {
    auto handle = GetHandle(identifier);
    if (handle == nullptr) {
        LOG_ERROR(Input, "Invalid handle {}", identifier.port);
        return Common::Input::PollingError::InvalidHandle;
    }

    switch (polling_mode) {
    case Common::Input::PollingMode::NFC:
        handle->SetNfcMode();
        break;
    case Common::Input::PollingMode::Active:
        handle->SetActiveMode();
        break;
    case Common::Input::PollingMode::Pasive:
        handle->SetPasiveMode();
        break;
    case Common::Input::PollingMode::Ring:
        handle->SetRingConMode();
        break;
    default:
        return Common::Input::PollingError::NotSupported;
    }

    return Common::Input::PollingError::None;
}

void Joycons::OnBatteryUpdate(std::size_t port, Joycon::ControllerType type,
                              Joycon::Battery value) {
    const auto identifier = GetIdentifier(port, type);
    if (value.charging != 0) {
        SetBattery(identifier, Common::Input::BatteryLevel::Charging);
        return;
    }

    Common::Input::BatteryLevel battery{value.status.Value()};
    switch (value.status) {
    case 0:
        battery = Common::Input::BatteryLevel::Empty;
        break;
    case 1:
        battery = Common::Input::BatteryLevel::Critical;
        break;
    case 2:
        battery = Common::Input::BatteryLevel::Low;
        break;
    case 3:
        battery = Common::Input::BatteryLevel::Medium;
        break;
    case 4:
    default:
        battery = Common::Input::BatteryLevel::Full;
        break;
    }
    SetBattery(identifier, battery);
}

void Joycons::OnColorUpdate(std::size_t port, Joycon::ControllerType type,
                            const Joycon::Color& value) {}

void Joycons::OnButtonUpdate(std::size_t port, Joycon::ControllerType type, int id, bool value) {
    const auto identifier = GetIdentifier(port, type);
    SetButton(identifier, id, value);
}

void Joycons::OnStickUpdate(std::size_t port, Joycon::ControllerType type, int id, f32 value) {
    const auto identifier = GetIdentifier(port, type);
    SetAxis(identifier, id, value);
}

void Joycons::OnMotionUpdate(std::size_t port, Joycon::ControllerType type, int id,
                             const Joycon::MotionData& value) {
    const auto identifier = GetIdentifier(port, type);
    BasicMotion motion_data{
        .gyro_x = value.gyro_x,
        .gyro_y = value.gyro_y,
        .gyro_z = value.gyro_z,
        .accel_x = value.accel_x,
        .accel_y = value.accel_y,
        .accel_z = value.accel_z,
        .delta_timestamp = 15000,
    };
    SetMotion(identifier, id, motion_data);
}

void Joycons::OnRingConUpdate(f32 ring_data) {
    // To simplify ring detection it will always be mapped to an empty identifier for all
    // controllers
    constexpr PadIdentifier identifier = {
        .guid = Common::UUID{},
        .port = 0,
        .pad = 0,
    };
    SetAxis(identifier, 100, ring_data);
}

void Joycons::OnAmiiboUpdate(std::size_t port, const std::vector<u8>& amiibo_data) {
    const auto identifier = GetIdentifier(port, Joycon::ControllerType::Right);
    SetNfc(identifier, {Common::Input::NfcState::NewAmiibo, amiibo_data});
}

std::shared_ptr<Joycon::JoyconDriver> Joycons::GetHandle(PadIdentifier identifier) const {
    auto is_handle_active = [&](std::shared_ptr<Joycon::JoyconDriver> device) {
        if (!device) {
            return false;
        }
        if (!device->IsConnected()) {
            return false;
        }
        if (device->GetDevicePort() == identifier.port) {
            return true;
        }
        return false;
    };
    const auto type = static_cast<Joycon::ControllerType>(identifier.pad);
    if (type == Joycon::ControllerType::Left) {
        for (const auto& device : left_joycons) {
            if (is_handle_active(device)) {
                return device;
            }
        }
    }
    if (type == Joycon::ControllerType::Right) {
        for (const auto& device : right_joycons) {
            if (is_handle_active(device)) {
                return device;
            }
        }
    }
    if (type == Joycon::ControllerType::Pro || type == Joycon::ControllerType::Grip) {
        for (const auto& device : pro_joycons) {
            if (is_handle_active(device)) {
                return device;
            }
        }
    }
    return nullptr;
}

PadIdentifier Joycons::GetIdentifier(std::size_t port, Joycon::ControllerType type) const {
    return {
        .guid = Common::UUID{Common::InvalidUUID},
        .port = port,
        .pad = static_cast<std::size_t>(type),
    };
}

std::vector<Common::ParamPackage> Joycons::GetInputDevices() const {
    std::vector<Common::ParamPackage> devices{};

    auto add_entry = [&](std::shared_ptr<Joycon::JoyconDriver> device) {
        if (!device) {
            return;
        }
        if (!device->IsConnected()) {
            return;
        }
        std::string name = fmt::format("{} {}", JoyconName(device->GetHandleDeviceType()),
                                       device->GetDevicePort());
        devices.emplace_back(Common::ParamPackage{
            {"engine", GetEngineName()},
            {"display", std::move(name)},
            {"port", std::to_string(device->GetDevicePort())},
            {"pad", std::to_string(static_cast<std::size_t>(device->GetHandleDeviceType()))},
        });
    };

    for (const auto& controller : left_joycons) {
        add_entry(controller);
    }
    for (const auto& controller : right_joycons) {
        add_entry(controller);
    }
    for (const auto& controller : pro_joycons) {
        add_entry(controller);
    }

    return devices;
}

ButtonMapping Joycons::GetButtonMappingForDevice(const Common::ParamPackage& params) {
    static constexpr std::array<std::pair<Settings::NativeButton::Values, Joycon::PadButton>, 20>
        switch_to_joycon_button = {
            std::pair{Settings::NativeButton::A, Joycon::PadButton::A},
            {Settings::NativeButton::B, Joycon::PadButton::B},
            {Settings::NativeButton::X, Joycon::PadButton::X},
            {Settings::NativeButton::Y, Joycon::PadButton::Y},
            {Settings::NativeButton::DLeft, Joycon::PadButton::Left},
            {Settings::NativeButton::DUp, Joycon::PadButton::Up},
            {Settings::NativeButton::DRight, Joycon::PadButton::Right},
            {Settings::NativeButton::DDown, Joycon::PadButton::Down},
            {Settings::NativeButton::SL, Joycon::PadButton::LeftSL},
            {Settings::NativeButton::SR, Joycon::PadButton::LeftSR},
            {Settings::NativeButton::L, Joycon::PadButton::L},
            {Settings::NativeButton::R, Joycon::PadButton::R},
            {Settings::NativeButton::ZL, Joycon::PadButton::ZL},
            {Settings::NativeButton::ZR, Joycon::PadButton::ZR},
            {Settings::NativeButton::Plus, Joycon::PadButton::Plus},
            {Settings::NativeButton::Minus, Joycon::PadButton::Minus},
            {Settings::NativeButton::Home, Joycon::PadButton::Home},
            {Settings::NativeButton::Screenshot, Joycon::PadButton::Capture},
            {Settings::NativeButton::LStick, Joycon::PadButton::StickL},
            {Settings::NativeButton::RStick, Joycon::PadButton::StickR},
        };

    if (!params.Has("port")) {
        return {};
    }

    ButtonMapping mapping{};
    for (const auto& [switch_button, joycon_button] : switch_to_joycon_button) {
        Common::ParamPackage button_params{};
        button_params.Set("engine", GetEngineName());
        button_params.Set("port", params.Get("port", 0));
        button_params.Set("button", static_cast<int>(joycon_button));
        mapping.insert_or_assign(switch_button, std::move(button_params));
    }

    return mapping;
}

AnalogMapping Joycons::GetAnalogMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("port")) {
        return {};
    }

    AnalogMapping mapping = {};
    Common::ParamPackage left_analog_params;
    left_analog_params.Set("engine", GetEngineName());
    left_analog_params.Set("port", params.Get("port", 0));
    left_analog_params.Set("axis_x", static_cast<int>(Joycon::PadAxes::LeftStickX));
    left_analog_params.Set("axis_y", static_cast<int>(Joycon::PadAxes::LeftStickY));
    mapping.insert_or_assign(Settings::NativeAnalog::LStick, std::move(left_analog_params));
    Common::ParamPackage right_analog_params;
    right_analog_params.Set("engine", GetEngineName());
    right_analog_params.Set("port", params.Get("port", 0));
    right_analog_params.Set("axis_x", static_cast<int>(Joycon::PadAxes::RightStickX));
    right_analog_params.Set("axis_y", static_cast<int>(Joycon::PadAxes::RightStickY));
    mapping.insert_or_assign(Settings::NativeAnalog::RStick, std::move(right_analog_params));
    return mapping;
}

MotionMapping Joycons::GetMotionMappingForDevice(const Common::ParamPackage& params) {
    if (!params.Has("port")) {
        return {};
    }

    MotionMapping mapping = {};
    Common::ParamPackage left_motion_params;
    left_motion_params.Set("engine", GetEngineName());
    left_motion_params.Set("port", params.Get("port", 0));
    left_motion_params.Set("motion", 0);
    mapping.insert_or_assign(Settings::NativeMotion::MotionLeft, std::move(left_motion_params));
    Common::ParamPackage right_Motion_params;
    right_Motion_params.Set("engine", GetEngineName());
    right_Motion_params.Set("port", params.Get("port", 0));
    right_Motion_params.Set("motion", 1);
    mapping.insert_or_assign(Settings::NativeMotion::MotionRight, std::move(right_Motion_params));
    return mapping;
}

Common::Input::ButtonNames Joycons::GetUIButtonName(const Common::ParamPackage& params) const {
    const auto button = static_cast<Joycon::PadButton>(params.Get("button", 0));
    switch (button) {
    case Joycon::PadButton::Left:
        return Common::Input::ButtonNames::ButtonLeft;
    case Joycon::PadButton::Right:
        return Common::Input::ButtonNames::ButtonRight;
    case Joycon::PadButton::Down:
        return Common::Input::ButtonNames::ButtonDown;
    case Joycon::PadButton::Up:
        return Common::Input::ButtonNames::ButtonUp;
    case Joycon::PadButton::LeftSL:
    case Joycon::PadButton::RightSL:
        return Common::Input::ButtonNames::TriggerSL;
    case Joycon::PadButton::LeftSR:
    case Joycon::PadButton::RightSR:
        return Common::Input::ButtonNames::TriggerSR;
    case Joycon::PadButton::L:
        return Common::Input::ButtonNames::TriggerL;
    case Joycon::PadButton::R:
        return Common::Input::ButtonNames::TriggerR;
    case Joycon::PadButton::ZL:
        return Common::Input::ButtonNames::TriggerZL;
    case Joycon::PadButton::ZR:
        return Common::Input::ButtonNames::TriggerZR;
    case Joycon::PadButton::A:
        return Common::Input::ButtonNames::ButtonA;
    case Joycon::PadButton::B:
        return Common::Input::ButtonNames::ButtonB;
    case Joycon::PadButton::X:
        return Common::Input::ButtonNames::ButtonX;
    case Joycon::PadButton::Y:
        return Common::Input::ButtonNames::ButtonY;
    case Joycon::PadButton::Plus:
        return Common::Input::ButtonNames::ButtonPlus;
    case Joycon::PadButton::Minus:
        return Common::Input::ButtonNames::ButtonMinus;
    case Joycon::PadButton::Home:
        return Common::Input::ButtonNames::ButtonHome;
    case Joycon::PadButton::Capture:
        return Common::Input::ButtonNames::ButtonCapture;
    case Joycon::PadButton::StickL:
        return Common::Input::ButtonNames::ButtonStickL;
    case Joycon::PadButton::StickR:
        return Common::Input::ButtonNames::ButtonStickR;
    default:
        return Common::Input::ButtonNames::Undefined;
    }
}

Common::Input::ButtonNames Joycons::GetUIName(const Common::ParamPackage& params) const {
    if (params.Has("button")) {
        return GetUIButtonName(params);
    }
    if (params.Has("axis")) {
        return Common::Input::ButtonNames::Value;
    }
    if (params.Has("motion")) {
        return Common::Input::ButtonNames::Engine;
    }

    return Common::Input::ButtonNames::Invalid;
}

std::string Joycons::JoyconName(Joycon::ControllerType type) const {
    switch (type) {
    case Joycon::ControllerType::Left:
        return "Left Joycon";
    case Joycon::ControllerType::Right:
        return "Right Joycon";
    case Joycon::ControllerType::Pro:
        return "Pro Controller";
    case Joycon::ControllerType::Grip:
        return "Grip Controller";
    default:
        return "Unknow Joycon";
    }
}
} // namespace InputCommon
