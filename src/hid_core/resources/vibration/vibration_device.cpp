// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hid_core/hid_result.h"
#include "hid_core/resources/npad/npad_types.h"
#include "hid_core/resources/npad/npad_vibration.h"
#include "hid_core/resources/vibration/vibration_device.h"

namespace Service::HID {

NpadVibrationDevice::NpadVibrationDevice() {}

Result NpadVibrationDevice::IncrementRefCounter() {
    ref_counter++;
    return ResultSuccess;
}

Result NpadVibrationDevice::DecrementRefCounter() {
    if (ref_counter > 0) {
        ref_counter--;
    }

    return ResultSuccess;
}

Result NpadVibrationDevice::SendVibrationValue(const Core::HID::VibrationValue& value) {
    if (ref_counter == 0) {
        return ResultVibrationNotInitialized;
    }
    if (!is_mounted) {
        return ResultSuccess;
    }

    f32 volume = 1.0f;
    const auto result = vibration_handler->GetVibrationVolume(volume);
    if (result.IsError()) {
        return result;
    }
    if (volume <= 0.0f) {
        // TODO: SendVibrationValue
        return ResultSuccess;
    }

    Core::HID::VibrationValue vibration_value = value;
    vibration_value.high_amplitude *= volume;
    vibration_value.low_amplitude *= volume;

    // TODO: SendVibrationValue
    return ResultSuccess;
}

Result NpadVibrationDevice::SendVibrationNotificationPattern([[maybe_unused]] u32 pattern) {
    if (!is_mounted) {
        return ResultSuccess;
    }

    f32 volume = 1.0f;
    const auto result = vibration_handler->GetVibrationVolume(volume);
    if (result.IsError()) {
        return result;
    }
    if (volume <= 0.0) {
        pattern = 0;
    }

    // return xcd_handle->SendVibrationNotificationPattern(pattern);
    return ResultSuccess;
}

Result NpadVibrationDevice::GetActualVibrationValue(Core::HID::VibrationValue& out_value) {
    if (ref_counter < 1) {
        return ResultVibrationNotInitialized;
    }

    out_value = Core::HID::DEFAULT_VIBRATION_VALUE;
    if (!is_mounted) {
        return ResultSuccess;
    }

    // TODO: SendVibrationValue
    return ResultSuccess;
}

} // namespace Service::HID
