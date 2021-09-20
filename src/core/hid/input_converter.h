// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#pragma once

namespace Input {
struct CallbackStatus;
};

namespace Core::HID {

/**
 * Converts raw input data into a valid battery status.
 *
 * @param Supported callbacks: Analog, Battery, Trigger.
 * @return A valid BatteryStatus object.
 */
Input::BatteryStatus TransformToBattery(const Input::CallbackStatus& callback);

/**
 * Converts raw input data into a valid button status. Applies invert properties to the output.
 *
 * @param Supported callbacks: Analog, Button, Trigger.
 * @return A valid TouchStatus object.
 */
Input::ButtonStatus TransformToButton(const Input::CallbackStatus& callback);

/**
 * Converts raw input data into a valid motion status.
 *
 * @param Supported callbacks: Motion.
 * @return A valid TouchStatus object.
 */
Input::MotionStatus TransformToMotion(const Input::CallbackStatus& callback);

/**
 * Converts raw input data into a valid stick status. Applies offset, deadzone, range and invert
 * properties to the output.
 *
 * @param Supported callbacks: Stick.
 * @return A valid StickStatus object.
 */
Input::StickStatus TransformToStick(const Input::CallbackStatus& callback);

/**
 * Converts raw input data into a valid touch status.
 *
 * @param Supported callbacks: Touch.
 * @return A valid TouchStatus object.
 */
Input::TouchStatus TransformToTouch(const Input::CallbackStatus& callback);

/**
 * Converts raw input data into a valid trigger status. Applies offset, deadzone, range and
 * invert properties to the output. Button status uses the threshold property if necessary.
 *
 * @param Supported callbacks: Analog, Button, Trigger.
 * @return A valid TriggerStatus object.
 */
Input::TriggerStatus TransformToTrigger(const Input::CallbackStatus& callback);

/**
 * Converts raw analog data into a valid analog value
 * @param An analog object containing raw data and properties, bool that determines if the value
 * needs to be clamped between -1.0f and 1.0f.
 */
void SanitizeAnalog(Input::AnalogStatus& analog, bool clamp_value);

/**
 * Converts raw stick data into a valid stick value
 * @param Two analog objects containing raw data and properties, bool that determines if the value
 * needs to be clamped into the unit circle.
 */
void SanitizeStick(Input::AnalogStatus& analog_x, Input::AnalogStatus& analog_y, bool clamp_value);

} // namespace Core::HID
