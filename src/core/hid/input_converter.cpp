// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#include <random>

#include "common/input.h"
#include "core/hid/input_converter.h"

namespace Core::HID {

Input::BatteryStatus TransformToBattery(const Input::CallbackStatus& callback) {
    Input::BatteryStatus battery{};
    switch (callback.type) {
    case Input::InputType::Analog:
    case Input::InputType::Trigger: {
        const auto value = TransformToTrigger(callback).analog.value;
        battery = Input::BatteryLevel::Empty;
        if (value > 0.2f) {
            battery = Input::BatteryLevel::Critical;
        }
        if (value > 0.4f) {
            battery = Input::BatteryLevel::Low;
        }
        if (value > 0.6f) {
            battery = Input::BatteryLevel::Medium;
        }
        if (value > 0.8f) {
            battery = Input::BatteryLevel::Full;
        }
        if (value >= 1.0f) {
            battery = Input::BatteryLevel::Charging;
        }
        break;
    }
    case Input::InputType::Battery:
        battery = callback.battery_status;
        break;
    default:
        LOG_ERROR(Input, "Conversion from type {} to battery not implemented", callback.type);
        break;
    }

    return battery;
}

Input::ButtonStatus TransformToButton(const Input::CallbackStatus& callback) {
    Input::ButtonStatus status{};
    switch (callback.type) {
    case Input::InputType::Analog:
    case Input::InputType::Trigger:
        status.value = TransformToTrigger(callback).pressed;
        break;
    case Input::InputType::Button:
        status = callback.button_status;
        break;
    default:
        LOG_ERROR(Input, "Conversion from type {} to button not implemented", callback.type);
        break;
    }

    if (status.inverted) {
        status.value = !status.value;
    }

    return status;
}

Input::MotionStatus TransformToMotion(const Input::CallbackStatus& callback) {
    Input::MotionStatus status{};
    switch (callback.type) {
    case Input::InputType::Button: {
        if (TransformToButton(callback).value) {
            std::random_device device;
            std::mt19937 gen(device());
            std::uniform_int_distribution<s16> distribution(-1000, 1000);
            Input::AnalogProperties properties{
                .deadzone = 0.0,
                .range = 1.0f,
                .offset = 0.0,
            };
            status.accel.x = {
                .value = 0,
                .raw_value = static_cast<f32>(distribution(gen)) * 0.001f,
                .properties = properties,
            };
            status.accel.y = {
                .value = 0,
                .raw_value = static_cast<f32>(distribution(gen)) * 0.001f,
                .properties = properties,
            };
            status.accel.z = {
                .value = 0,
                .raw_value = static_cast<f32>(distribution(gen)) * 0.001f,
                .properties = properties,
            };
            status.gyro.x = {
                .value = 0,
                .raw_value = static_cast<f32>(distribution(gen)) * 0.001f,
                .properties = properties,
            };
            status.gyro.y = {
                .value = 0,
                .raw_value = static_cast<f32>(distribution(gen)) * 0.001f,
                .properties = properties,
            };
            status.gyro.z = {
                .value = 0,
                .raw_value = static_cast<f32>(distribution(gen)) * 0.001f,
                .properties = properties,
            };
        }
        break;
    }
    case Input::InputType::Motion:
        status = callback.motion_status;
        break;
    default:
        LOG_ERROR(Input, "Conversion from type {} to motion not implemented", callback.type);
        break;
    }
    SanitizeAnalog(status.accel.x, false);
    SanitizeAnalog(status.accel.y, false);
    SanitizeAnalog(status.accel.z, false);
    SanitizeAnalog(status.gyro.x, false);
    SanitizeAnalog(status.gyro.y, false);
    SanitizeAnalog(status.gyro.z, false);

    return status;
}

Input::StickStatus TransformToStick(const Input::CallbackStatus& callback) {
    Input::StickStatus status{};

    switch (callback.type) {
    case Input::InputType::Stick:
        status = callback.stick_status;
        break;
    default:
        LOG_ERROR(Input, "Conversion from type {} to stick not implemented", callback.type);
        break;
    }

    SanitizeStick(status.x, status.y, true);
    const Input::AnalogProperties& properties_x = status.x.properties;
    const Input::AnalogProperties& properties_y = status.y.properties;
    const float x = status.x.value;
    const float y = status.y.value;

    // Set directional buttons
    status.right = x > properties_x.threshold;
    status.left = x < -properties_x.threshold;
    status.up = y > properties_y.threshold;
    status.down = y < -properties_y.threshold;

    return status;
}

Input::TouchStatus TransformToTouch(const Input::CallbackStatus& callback) {
    Input::TouchStatus status{};

    switch (callback.type) {
    case Input::InputType::Touch:
        status = callback.touch_status;
        break;
    default:
        LOG_ERROR(Input, "Conversion from type {} to touch not implemented", callback.type);
        break;
    }

    SanitizeAnalog(status.x, true);
    SanitizeAnalog(status.y, true);
    float& x = status.x.value;
    float& y = status.y.value;

    // Adjust if value is inverted
    x = status.x.properties.inverted ? 1.0f + x : x;
    y = status.y.properties.inverted ? 1.0f + y : y;

    // clamp value
    x = std::clamp(x, 0.0f, 1.0f);
    y = std::clamp(y, 0.0f, 1.0f);

    if (status.pressed.inverted) {
        status.pressed.value = !status.pressed.value;
    }

    return status;
}

Input::TriggerStatus TransformToTrigger(const Input::CallbackStatus& callback) {
    Input::TriggerStatus status{};
    float& raw_value = status.analog.raw_value;
    bool calculate_button_value = true;

    switch (callback.type) {
    case Input::InputType::Analog:
        status.analog.properties = callback.analog_status.properties;
        raw_value = callback.analog_status.raw_value;
        break;
    case Input::InputType::Button:
        status.analog.properties.range = 1.0f;
        status.analog.properties.inverted = callback.button_status.inverted;
        raw_value = callback.button_status.value ? 1.0f : 0.0f;
        break;
    case Input::InputType::Trigger:
        status = callback.trigger_status;
        calculate_button_value = false;
        break;
    default:
        LOG_ERROR(Input, "Conversion from type {} to trigger not implemented", callback.type);
        break;
    }

    SanitizeAnalog(status.analog, true);
    const Input::AnalogProperties& properties = status.analog.properties;
    float& value = status.analog.value;

    // Set button status
    if (calculate_button_value) {
        status.pressed = value > properties.threshold;
    }

    // Adjust if value is inverted
    value = properties.inverted ? 1.0f + value : value;

    // clamp value
    value = std::clamp(value, 0.0f, 1.0f);

    return status;
}

void SanitizeAnalog(Input::AnalogStatus& analog, bool clamp_value) {
    const Input::AnalogProperties& properties = analog.properties;
    float& raw_value = analog.raw_value;
    float& value = analog.value;

    if (!std::isnormal(raw_value)) {
        raw_value = 0;
    }

    // Apply center offset
    raw_value -= properties.offset;

    // Set initial values to be formated
    value = raw_value;

    // Calculate vector size
    const float r = std::abs(value);

    // Return zero if value is smaller than the deadzone
    if (r <= properties.deadzone || properties.deadzone == 1.0f) {
        analog.value = 0;
        return;
    }

    // Adjust range of value
    const float deadzone_factor =
        1.0f / r * (r - properties.deadzone) / (1.0f - properties.deadzone);
    value = value * deadzone_factor / properties.range;

    // Invert direction if needed
    if (properties.inverted) {
        value = -value;
    }

    // Clamp value
    if (clamp_value) {
        value = std::clamp(value, -1.0f, 1.0f);
    }
}

void SanitizeStick(Input::AnalogStatus& analog_x, Input::AnalogStatus& analog_y, bool clamp_value) {
    const Input::AnalogProperties& properties_x = analog_x.properties;
    const Input::AnalogProperties& properties_y = analog_y.properties;
    float& raw_x = analog_x.raw_value;
    float& raw_y = analog_y.raw_value;
    float& x = analog_x.value;
    float& y = analog_y.value;

    if (!std::isnormal(raw_x)) {
        raw_x = 0;
    }
    if (!std::isnormal(raw_y)) {
        raw_y = 0;
    }

    // Apply center offset
    raw_x += properties_x.offset;
    raw_y += properties_y.offset;

    // Apply X scale correction from offset
    if (std::abs(properties_x.offset) < 0.5f) {
        if (raw_x > 0) {
            raw_x /= 1 + properties_x.offset;
        } else {
            raw_x /= 1 - properties_x.offset;
        }
    }

    // Apply Y scale correction from offset
    if (std::abs(properties_y.offset) < 0.5f) {
        if (raw_y > 0) {
            raw_y /= 1 + properties_y.offset;
        } else {
            raw_y /= 1 - properties_y.offset;
        }
    }

    // Invert direction if needed
    raw_x = properties_x.inverted ? -raw_x : raw_x;
    raw_y = properties_y.inverted ? -raw_y : raw_y;

    // Set initial values to be formated
    x = raw_x;
    y = raw_y;

    // Calculate vector size
    float r = x * x + y * y;
    r = std::sqrt(r);

    // TODO(German77): Use deadzone and range of both axis

    // Return zero if values are smaller than the deadzone
    if (r <= properties_x.deadzone || properties_x.deadzone >= 1.0f) {
        x = 0;
        y = 0;
        return;
    }

    // Adjust range of joystick
    const float deadzone_factor =
        1.0f / r * (r - properties_x.deadzone) / (1.0f - properties_x.deadzone);
    x = x * deadzone_factor / properties_x.range;
    y = y * deadzone_factor / properties_x.range;
    r = r * deadzone_factor / properties_x.range;

    // Normalize joystick
    if (clamp_value && r > 1.0f) {
        x /= r;
        y /= r;
    }
}

} // namespace Core::HID
