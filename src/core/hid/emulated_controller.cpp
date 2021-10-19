// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#include <fmt/format.h>

#include "core/hid/emulated_controller.h"
#include "core/hid/input_converter.h"

namespace Core::HID {
constexpr s32 HID_JOYSTICK_MAX = 0x7fff;
constexpr s32 HID_TRIGGER_MAX = 0x7fff;

EmulatedController::EmulatedController(NpadIdType npad_id_type_) : npad_id_type(npad_id_type_) {}

EmulatedController::~EmulatedController() = default;

NpadType EmulatedController::MapSettingsTypeToNPad(Settings::ControllerType type) {
    switch (type) {
    case Settings::ControllerType::ProController:
        return NpadType::ProController;
    case Settings::ControllerType::DualJoyconDetached:
        return NpadType::JoyconDual;
    case Settings::ControllerType::LeftJoycon:
        return NpadType::JoyconLeft;
    case Settings::ControllerType::RightJoycon:
        return NpadType::JoyconRight;
    case Settings::ControllerType::Handheld:
        return NpadType::Handheld;
    case Settings::ControllerType::GameCube:
        return NpadType::GameCube;
    default:
        return NpadType::ProController;
    }
}

Settings::ControllerType EmulatedController::MapNPadToSettingsType(NpadType type) {
    switch (type) {
    case NpadType::ProController:
        return Settings::ControllerType::ProController;
    case NpadType::JoyconDual:
        return Settings::ControllerType::DualJoyconDetached;
    case NpadType::JoyconLeft:
        return Settings::ControllerType::LeftJoycon;
    case NpadType::JoyconRight:
        return Settings::ControllerType::RightJoycon;
    case NpadType::Handheld:
        return Settings::ControllerType::Handheld;
    case NpadType::GameCube:
        return Settings::ControllerType::GameCube;
    default:
        return Settings::ControllerType::ProController;
    }
}

void EmulatedController::ReloadFromSettings() {
    //LOG_ERROR(Service_HID, "reload config from settings {}", NpadIdTypeToIndex(npad_id_type));
    const auto player_index = NpadIdTypeToIndex(npad_id_type);
    const auto& player = Settings::values.players.GetValue()[player_index];

    for (std::size_t index = 0; index < player.buttons.size(); ++index) {
        button_params[index] = Common::ParamPackage(player.buttons[index]);
    }
    for (std::size_t index = 0; index < player.analogs.size(); ++index) {
        stick_params[index] = Common::ParamPackage(player.analogs[index]);
    }
    for (std::size_t index = 0; index < player.motions.size(); ++index) {
        motion_params[index] = Common::ParamPackage(player.motions[index]);
    }

    controller.colors_state.left = {
        .body = player.body_color_left,
        .button = player.button_color_left,
    };

    controller.colors_state.right = {
        .body = player.body_color_right,
        .button = player.button_color_right,
    };

    controller.colors_state.fullkey = controller.colors_state.left;

    SetNpadType(MapSettingsTypeToNPad(player.controller_type));

    if (player.connected) {
        Connect();
    } else {
        Disconnect();
    }

    ReloadInput();
}

void EmulatedController::ReloadInput() {
    //LOG_ERROR(Service_HID, "reload config {}", NpadIdTypeToIndex(npad_id_type));
    // If you load any device here add the equivalent to the UnloadInput() function
    const auto player_index = NpadIdTypeToIndex(npad_id_type);
    const auto left_side = button_params[Settings::NativeButton::ZL];
    const auto right_side = button_params[Settings::NativeButton::ZR];

    std::transform(button_params.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                   button_params.begin() + Settings::NativeButton::BUTTON_NS_END,
                   button_devices.begin(), Input::CreateDevice<Input::InputDevice>);
    std::transform(stick_params.begin() + Settings::NativeAnalog::STICK_HID_BEGIN,
                   stick_params.begin() + Settings::NativeAnalog::STICK_HID_END,
                   stick_devices.begin(), Input::CreateDevice<Input::InputDevice>);
    std::transform(motion_params.begin() + Settings::NativeMotion::MOTION_HID_BEGIN,
                   motion_params.begin() + Settings::NativeMotion::MOTION_HID_END,
                   motion_devices.begin(), Input::CreateDevice<Input::InputDevice>);

    trigger_devices[0] =
        Input::CreateDevice<Input::InputDevice>(button_params[Settings::NativeButton::ZL]);
    trigger_devices[1] =
        Input::CreateDevice<Input::InputDevice>(button_params[Settings::NativeButton::ZR]);

    battery_devices[0] = Input::CreateDevice<Input::InputDevice>(left_side);
    battery_devices[1] = Input::CreateDevice<Input::InputDevice>(right_side);

    button_params[Settings::NativeButton::ZL].Set("output", true);
    output_devices[0] =
        Input::CreateDevice<Input::OutputDevice>(button_params[Settings::NativeButton::ZL]);

    for (std::size_t index = 0; index < button_devices.size(); ++index) {
        if (!button_devices[index]) {
            continue;
        }
        Input::InputCallback button_callback{
            [this, index](Input::CallbackStatus callback) { SetButton(callback, index); }};
        button_devices[index]->SetCallback(button_callback);
    }

    for (std::size_t index = 0; index < stick_devices.size(); ++index) {
        if (!stick_devices[index]) {
            continue;
        }
        Input::InputCallback stick_callback{
            [this, index](Input::CallbackStatus callback) { SetStick(callback, index); }};
        stick_devices[index]->SetCallback(stick_callback);
    }

    for (std::size_t index = 0; index < trigger_devices.size(); ++index) {
        if (!trigger_devices[index]) {
            continue;
        }
        Input::InputCallback trigger_callback{
            [this, index](Input::CallbackStatus callback) { SetTrigger(callback, index); }};
        trigger_devices[index]->SetCallback(trigger_callback);
    }

    for (std::size_t index = 0; index < battery_devices.size(); ++index) {
        if (!battery_devices[index]) {
            continue;
        }
        Input::InputCallback battery_callback{
            [this, index](Input::CallbackStatus callback) { SetBattery(callback, index); }};
        battery_devices[index]->SetCallback(battery_callback);
    }

    for (std::size_t index = 0; index < motion_devices.size(); ++index) {
        if (!motion_devices[index]) {
            continue;
        }
        Input::InputCallback motion_callback{
            [this, index](Input::CallbackStatus callback) { SetMotion(callback, index); }};
        motion_devices[index]->SetCallback(motion_callback);
    }
}

void EmulatedController::UnloadInput() {
    for (auto& button : button_devices) {
        button.reset();
    }
    for (auto& stick : stick_devices) {
        stick.reset();
    }
    for (auto& motion : motion_devices) {
        motion.reset();
    }
    for (auto& trigger : trigger_devices) {
        trigger.reset();
    }
    for (auto& battery : battery_devices) {
        battery.reset();
    }
    for (auto& output : output_devices) {
        output.reset();
    }
}

void EmulatedController::EnableConfiguration() {
    is_configuring = true;
    temporary_is_connected = is_connected;
    temporary_npad_type = npad_type;
}

void EmulatedController::DisableConfiguration() {
    is_configuring = false;

    // Apply temporary npad type to the real controller
    if (temporary_npad_type != npad_type) {
        if (is_connected) {
            Disconnect();
        }
        SetNpadType(temporary_npad_type);
    }

    // Apply temporary connected status to the real controller
    if (temporary_is_connected != is_connected) {
        if (temporary_is_connected) {
            Connect();
            return;
        }
        Disconnect();
    }
}

bool EmulatedController::IsConfiguring() const {
    return is_configuring;
}

void EmulatedController::SaveCurrentConfig() {
    const auto player_index = NpadIdTypeToIndex(npad_id_type);
    auto& player = Settings::values.players.GetValue()[player_index];
    player.connected = is_connected;
    player.controller_type = MapNPadToSettingsType(npad_type);
    for (std::size_t index = 0; index < player.buttons.size(); ++index) {
        player.buttons[index] = button_params[index].Serialize();
    }
    for (std::size_t index = 0; index < player.analogs.size(); ++index) {
        player.analogs[index] = stick_params[index].Serialize();
    }
    for (std::size_t index = 0; index < player.motions.size(); ++index) {
        player.motions[index] = motion_params[index].Serialize();
    }
}

void EmulatedController::RestoreConfig() {
    if (!is_configuring) {
        return;
    }
    ReloadFromSettings();
}

std::vector<Common::ParamPackage> EmulatedController::GetMappedDevices() const {
    std::vector<Common::ParamPackage> devices;
    for (const auto& param : button_params) {
        if (!param.Has("engine")) {
            continue;
        }
        const auto devices_it = std::find_if(
            devices.begin(), devices.end(), [param](const Common::ParamPackage param_) {
                return param.Get("engine", "") == param_.Get("engine", "") &&
                       param.Get("guid", "") == param_.Get("guid", "") &&
                       param.Get("port", "") == param_.Get("port", "");
            });
        if (devices_it != devices.end()) {
            continue;
        }
        Common::ParamPackage device{};
        device.Set("engine", param.Get("engine", ""));
        device.Set("guid", param.Get("guid", ""));
        device.Set("port", param.Get("port", ""));
        devices.push_back(device);
    }

    for (const auto& param : stick_params) {
        if (!param.Has("engine")) {
            continue;
        }
        if (param.Get("engine", "") == "analog_from_button") {
            continue;
        }
        const auto devices_it = std::find_if(
            devices.begin(), devices.end(), [param](const Common::ParamPackage param_) {
                return param.Get("engine", "") == param_.Get("engine", "") &&
                       param.Get("guid", "") == param_.Get("guid", "") &&
                       param.Get("port", "") == param_.Get("port", "");
            });
        if (devices_it != devices.end()) {
            continue;
        }
        Common::ParamPackage device{};
        device.Set("engine", param.Get("engine", ""));
        device.Set("guid", param.Get("guid", ""));
        device.Set("port", param.Get("port", ""));
        devices.push_back(device);
    }
    return devices;
}

Common::ParamPackage EmulatedController::GetButtonParam(std::size_t index) const {
    if (index >= button_params.size()) {
        return {};
    }
    return button_params[index];
}

Common::ParamPackage EmulatedController::GetStickParam(std::size_t index) const {
    if (index >= stick_params.size()) {
        return {};
    }
    return stick_params[index];
}

Common::ParamPackage EmulatedController::GetMotionParam(std::size_t index) const {
    if (index >= motion_params.size()) {
        return {};
    }
    return motion_params[index];
}

void EmulatedController::SetButtonParam(std::size_t index, Common::ParamPackage param) {
    if (index >= button_params.size()) {
        return;
    }
    button_params[index] = param;
    ReloadInput();
}

void EmulatedController::SetStickParam(std::size_t index, Common::ParamPackage param) {
    if (index >= stick_params.size()) {
        return;
    }
    stick_params[index] = param;
    ReloadInput();
}

void EmulatedController::SetMotionParam(std::size_t index, Common::ParamPackage param) {
    if (index >= motion_params.size()) {
        return;
    }
    motion_params[index] = param;
    ReloadInput();
}

void EmulatedController::SetButton(Input::CallbackStatus callback, std::size_t index) {
    if (index >= controller.button_values.size()) {
        return;
    }
    std::lock_guard lock{mutex};
    bool value_changed = false;
    const auto new_status = TransformToButton(callback);
    auto& current_status = controller.button_values[index];
    current_status.toggle = new_status.toggle;

    // Update button status with current
    if (!current_status.toggle) {
        current_status.locked = false;
        if (current_status.value != new_status.value) {
            current_status.value = new_status.value;
            value_changed = true;
        }
    } else {
        // Toggle button and lock status
        if (new_status.value && !current_status.locked) {
            current_status.locked = true;
            current_status.value = !current_status.value;
            value_changed = true;
        }

        // Unlock button ready for next press
        if (!new_status.value && current_status.locked) {
            current_status.locked = false;
        }
    }

    if (!value_changed) {
        return;
    }

    if (is_configuring) {
        controller.npad_button_state.raw = NpadButton::None;
        controller.debug_pad_button_state.raw = 0;
        TriggerOnChange(ControllerTriggerType::Button);
        return;
    }

    switch (index) {
    case Settings::NativeButton::A:
        controller.npad_button_state.a.Assign(current_status.value);
        controller.debug_pad_button_state.a.Assign(current_status.value);
        break;
    case Settings::NativeButton::B:
        controller.npad_button_state.b.Assign(current_status.value);
        controller.debug_pad_button_state.b.Assign(current_status.value);
        break;
    case Settings::NativeButton::X:
        controller.npad_button_state.x.Assign(current_status.value);
        controller.debug_pad_button_state.x.Assign(current_status.value);
        break;
    case Settings::NativeButton::Y:
        controller.npad_button_state.y.Assign(current_status.value);
        controller.debug_pad_button_state.y.Assign(current_status.value);
        break;
    case Settings::NativeButton::LStick:
        controller.npad_button_state.stick_l.Assign(current_status.value);
        break;
    case Settings::NativeButton::RStick:
        controller.npad_button_state.stick_r.Assign(current_status.value);
        break;
    case Settings::NativeButton::L:
        controller.npad_button_state.l.Assign(current_status.value);
        controller.debug_pad_button_state.l.Assign(current_status.value);
        break;
    case Settings::NativeButton::R:
        controller.npad_button_state.r.Assign(current_status.value);
        controller.debug_pad_button_state.r.Assign(current_status.value);
        break;
    case Settings::NativeButton::ZL:
        controller.npad_button_state.zl.Assign(current_status.value);
        controller.debug_pad_button_state.zl.Assign(current_status.value);
        break;
    case Settings::NativeButton::ZR:
        controller.npad_button_state.zr.Assign(current_status.value);
        controller.debug_pad_button_state.zr.Assign(current_status.value);
        break;
    case Settings::NativeButton::Plus:
        controller.npad_button_state.plus.Assign(current_status.value);
        controller.debug_pad_button_state.plus.Assign(current_status.value);
        break;
    case Settings::NativeButton::Minus:
        controller.npad_button_state.minus.Assign(current_status.value);
        controller.debug_pad_button_state.minus.Assign(current_status.value);
        break;
    case Settings::NativeButton::DLeft:
        controller.npad_button_state.left.Assign(current_status.value);
        controller.debug_pad_button_state.d_left.Assign(current_status.value);
        break;
    case Settings::NativeButton::DUp:
        controller.npad_button_state.up.Assign(current_status.value);
        controller.debug_pad_button_state.d_up.Assign(current_status.value);
        break;
    case Settings::NativeButton::DRight:
        controller.npad_button_state.right.Assign(current_status.value);
        controller.debug_pad_button_state.d_right.Assign(current_status.value);
        break;
    case Settings::NativeButton::DDown:
        controller.npad_button_state.down.Assign(current_status.value);
        controller.debug_pad_button_state.d_down.Assign(current_status.value);
        break;
    case Settings::NativeButton::SL:
        controller.npad_button_state.left_sl.Assign(current_status.value);
        controller.npad_button_state.right_sl.Assign(current_status.value);
        break;
    case Settings::NativeButton::SR:
        controller.npad_button_state.left_sr.Assign(current_status.value);
        controller.npad_button_state.right_sr.Assign(current_status.value);
        break;
    case Settings::NativeButton::Home:
    case Settings::NativeButton::Screenshot:
        break;
    }
    TriggerOnChange(ControllerTriggerType::Button);
}

void EmulatedController::SetStick(Input::CallbackStatus callback, std::size_t index) {
    if (index >= controller.stick_values.size()) {
        return;
    }
    std::lock_guard lock{mutex};
    controller.stick_values[index] = TransformToStick(callback);

    if (is_configuring) {
        controller.analog_stick_state.left = {};
        controller.analog_stick_state.right = {};
        TriggerOnChange(ControllerTriggerType::Stick);
        return;
    }

    const AnalogStickState stick{
        .x = static_cast<s32>(controller.stick_values[index].x.value * HID_JOYSTICK_MAX),
        .y = static_cast<s32>(controller.stick_values[index].y.value * HID_JOYSTICK_MAX),
    };

    switch (index) {
    case Settings::NativeAnalog::LStick:
        controller.analog_stick_state.left = stick;
        controller.npad_button_state.stick_l_left.Assign(controller.stick_values[index].left);
        controller.npad_button_state.stick_l_up.Assign(controller.stick_values[index].up);
        controller.npad_button_state.stick_l_right.Assign(controller.stick_values[index].right);
        controller.npad_button_state.stick_l_down.Assign(controller.stick_values[index].down);
        break;
    case Settings::NativeAnalog::RStick:
        controller.analog_stick_state.right = stick;
        controller.npad_button_state.stick_r_left.Assign(controller.stick_values[index].left);
        controller.npad_button_state.stick_r_up.Assign(controller.stick_values[index].up);
        controller.npad_button_state.stick_r_right.Assign(controller.stick_values[index].right);
        controller.npad_button_state.stick_r_down.Assign(controller.stick_values[index].down);
        break;
    }

    TriggerOnChange(ControllerTriggerType::Stick);
}

void EmulatedController::SetTrigger(Input::CallbackStatus callback, std::size_t index) {
    if (index >= controller.trigger_values.size()) {
        return;
    }
    std::lock_guard lock{mutex};
    controller.trigger_values[index] = TransformToTrigger(callback);

    if (is_configuring) {
        controller.gc_trigger_state.left = 0;
        controller.gc_trigger_state.right = 0;
        TriggerOnChange(ControllerTriggerType::Trigger);
        return;
    }

    const auto trigger = controller.trigger_values[index];

    switch (index) {
    case Settings::NativeTrigger::LTrigger:
        controller.gc_trigger_state.left = static_cast<s32>(trigger.analog.value * HID_TRIGGER_MAX);
        controller.npad_button_state.zl.Assign(trigger.pressed);
        break;
    case Settings::NativeTrigger::RTrigger:
        controller.gc_trigger_state.right =
            static_cast<s32>(trigger.analog.value * HID_TRIGGER_MAX);
        controller.npad_button_state.zr.Assign(trigger.pressed);
        break;
    }

    TriggerOnChange(ControllerTriggerType::Trigger);
}

void EmulatedController::SetMotion(Input::CallbackStatus callback, std::size_t index) {
    if (index >= controller.motion_values.size()) {
        return;
    }
    std::lock_guard lock{mutex};
    auto& raw_status = controller.motion_values[index].raw_status;
    auto& emulated = controller.motion_values[index].emulated;

    raw_status = TransformToMotion(callback);
    emulated.SetAcceleration(Common::Vec3f{
        raw_status.accel.x.value,
        raw_status.accel.y.value,
        raw_status.accel.z.value,
    });
    emulated.SetGyroscope(Common::Vec3f{
        raw_status.gyro.x.value,
        raw_status.gyro.y.value,
        raw_status.gyro.z.value,
    });
    emulated.UpdateRotation(raw_status.delta_timestamp);
    emulated.UpdateOrientation(raw_status.delta_timestamp);

    if (is_configuring) {
        TriggerOnChange(ControllerTriggerType::Motion);
        return;
    }

    auto& motion = controller.motion_state[index];
    motion.accel = emulated.GetAcceleration();
    motion.gyro = emulated.GetGyroscope();
    motion.rotation = emulated.GetGyroscope();
    motion.orientation = emulated.GetOrientation();
    motion.is_at_rest = emulated.IsMoving(motion_sensitivity);

    TriggerOnChange(ControllerTriggerType::Motion);
}

void EmulatedController::SetBattery(Input::CallbackStatus callback, std::size_t index) {
    if (index >= controller.battery_values.size()) {
        return;
    }
    std::lock_guard lock{mutex};
    controller.battery_values[index] = TransformToBattery(callback);

    if (is_configuring) {
        TriggerOnChange(ControllerTriggerType::Battery);
        return;
    }

    bool is_charging = false;
    bool is_powered = false;
    BatteryLevel battery_level = 0;
    switch (controller.battery_values[index]) {
    case Input::BatteryLevel::Charging:
        is_charging = true;
        is_powered = true;
        battery_level = 6;
        break;
    case Input::BatteryLevel::Medium:
        battery_level = 6;
        break;
    case Input::BatteryLevel::Low:
        battery_level = 4;
        break;
    case Input::BatteryLevel::Critical:
        battery_level = 2;
        break;
    case Input::BatteryLevel::Empty:
        battery_level = 0;
        break;
    case Input::BatteryLevel::Full:
    default:
        is_powered = true;
        battery_level = 8;
        break;
    }

    switch (index) {
    case 0:
        controller.battery_state.left = {
            .is_powered = is_powered,
            .is_charging = is_charging,
            .battery_level = battery_level,
        };
        break;
    case 1:
        controller.battery_state.right = {
            .is_powered = is_powered,
            .is_charging = is_charging,
            .battery_level = battery_level,
        };
        break;
    case 2:
        controller.battery_state.dual = {
            .is_powered = is_powered,
            .is_charging = is_charging,
            .battery_level = battery_level,
        };
        break;
    }
    TriggerOnChange(ControllerTriggerType::Battery);
}

bool EmulatedController::SetVibration(std::size_t device_index, VibrationValue vibration) {
    if (!output_devices[device_index]) {
        return false;
    }

    const Input::VibrationStatus status = {
        .low_amplitude = vibration.high_amplitude,
        .low_frequency = vibration.high_amplitude,
        .high_amplitude = vibration.high_amplitude,
        .high_frequency = vibration.high_amplitude,
    };
    return output_devices[device_index]->SetVibration(status) == Input::VibrationError::None;
}

bool EmulatedController::TestVibration(std::size_t device_index) {
    if (!output_devices[device_index]) {
        return false;
    }

    // Send a slight vibration to test for rumble support
    constexpr Input::VibrationStatus status = {
        .low_amplitude = 0.001f,
        .low_frequency = 160.0f,
        .high_amplitude = 0.001f,
        .high_frequency = 320.0f,
    };
    return output_devices[device_index]->SetVibration(status) == Input::VibrationError::None;
}

void EmulatedController::SetLedPattern() {
    for (auto& device : output_devices) {
        if (!device) {
            continue;
        }

        const LedPattern pattern = GetLedPattern();
        const Input::LedStatus status = {
            .led_1 = pattern.position1 != 0,
            .led_2 = pattern.position2 != 0,
            .led_3 = pattern.position3 != 0,
            .led_4 = pattern.position4 != 0,
        };
        device->SetLED(status);
    }
}

void EmulatedController::Connect() {
    {
        std::lock_guard lock{mutex};
        if (is_configuring) {
            temporary_is_connected = true;
            TriggerOnChange(ControllerTriggerType::Connected);
            return;
        }

        if (is_connected) {
            return;
        }
        is_connected = true;
    }
    LOG_ERROR(Service_HID, "Connected controller {}", NpadIdTypeToIndex(npad_id_type));
    TriggerOnChange(ControllerTriggerType::Connected);
}

void EmulatedController::Disconnect() {
    {
        std::lock_guard lock{mutex};
        if (is_configuring) {
            temporary_is_connected = false;
            LOG_ERROR(Service_HID, "Disconnected temporal controller {}",
                      NpadIdTypeToIndex(npad_id_type));
            TriggerOnChange(ControllerTriggerType::Disconnected);
            return;
        }

        if (!is_connected) {
            return;
        }
        is_connected = false;
    }
    LOG_ERROR(Service_HID, "Disconnected controller {}", NpadIdTypeToIndex(npad_id_type));
    TriggerOnChange(ControllerTriggerType::Disconnected);
}

bool EmulatedController::IsConnected(bool temporary) const {
    if (temporary) {
        return temporary_is_connected;
    }
    return is_connected;
}

bool EmulatedController::IsVibrationEnabled() const {
    return is_vibration_enabled;
}

NpadIdType EmulatedController::GetNpadIdType() const {
    return npad_id_type;
}

NpadType EmulatedController::GetNpadType(bool temporary) const {
    if (temporary) {
        return temporary_npad_type;
    }
    return npad_type;
}

void EmulatedController::SetNpadType(NpadType npad_type_) {
    {
        std::lock_guard lock{mutex};

        if (is_configuring) {
            if (temporary_npad_type == npad_type_) {
                return;
            }
            temporary_npad_type = npad_type_;
            TriggerOnChange(ControllerTriggerType::Type);
            return;
        }

        if (npad_type == npad_type_) {
            return;
        }
        if (is_connected) {
            LOG_WARNING(Service_HID, "Controller {} type changed while it's connected",
                        NpadIdTypeToIndex(npad_id_type));
        }
        npad_type = npad_type_;
    }
    TriggerOnChange(ControllerTriggerType::Type);
}

LedPattern EmulatedController::GetLedPattern() const {
    switch (npad_id_type) {
    case NpadIdType::Player1:
        return LedPattern{1, 0, 0, 0};
    case NpadIdType::Player2:
        return LedPattern{1, 1, 0, 0};
    case NpadIdType::Player3:
        return LedPattern{1, 1, 1, 0};
    case NpadIdType::Player4:
        return LedPattern{1, 1, 1, 1};
    case NpadIdType::Player5:
        return LedPattern{1, 0, 0, 1};
    case NpadIdType::Player6:
        return LedPattern{1, 0, 1, 0};
    case NpadIdType::Player7:
        return LedPattern{1, 0, 1, 1};
    case NpadIdType::Player8:
        return LedPattern{0, 1, 1, 0};
    default:
        return LedPattern{0, 0, 0, 0};
    }
}

ButtonValues EmulatedController::GetButtonsValues() const {
    return controller.button_values;
}

SticksValues EmulatedController::GetSticksValues() const {
    return controller.stick_values;
}

TriggerValues EmulatedController::GetTriggersValues() const {
    return controller.trigger_values;
}

ControllerMotionValues EmulatedController::GetMotionValues() const {
    return controller.motion_values;
}

ColorValues EmulatedController::GetColorsValues() const {
    return controller.color_values;
}

BatteryValues EmulatedController::GetBatteryValues() const {
    return controller.battery_values;
}

NpadButtonState EmulatedController::GetNpadButtons() const {
    if (is_configuring) {
        return {};
    }
    return controller.npad_button_state;
}

DebugPadButton EmulatedController::GetDebugPadButtons() const {
    if (is_configuring) {
        return {};
    }
    return controller.debug_pad_button_state;
}

AnalogSticks EmulatedController::GetSticks() const {
    if (is_configuring) {
        return {};
    }
    return controller.analog_stick_state;
}

NpadGcTriggerState EmulatedController::GetTriggers() const {
    if (is_configuring) {
        return {};
    }
    return controller.gc_trigger_state;
}

MotionState EmulatedController::GetMotions() const {
    return controller.motion_state;
}

ControllerColors EmulatedController::GetColors() const {
    return controller.colors_state;
}

BatteryLevelState EmulatedController::GetBattery() const {
    return controller.battery_state;
}

void EmulatedController::TriggerOnChange(ControllerTriggerType type) {
    for (const std::pair<int, ControllerUpdateCallback> poller_pair : callback_list) {
        const ControllerUpdateCallback& poller = poller_pair.second;
        if (poller.on_change) {
            poller.on_change(type);
        }
    }
}

int EmulatedController::SetCallback(ControllerUpdateCallback update_callback) {
    std::lock_guard lock{mutex};
    callback_list.insert_or_assign(last_callback_key, update_callback);
    return last_callback_key++;
}

void EmulatedController::DeleteCallback(int key) {
    std::lock_guard lock{mutex};
    if (!callback_list.contains(key)) {
        LOG_ERROR(Input, "Tried to delete non-existent callback {}", key);
        return;
    }
    callback_list.erase(key);
}
} // namespace Core::HID
