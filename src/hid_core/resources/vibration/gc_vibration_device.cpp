// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hid_core/hid_result.h"
#include "hid_core/resources/npad/npad_types.h"
#include "hid_core/resources/npad/npad_vibration.h"
#include "hid_core/resources/vibration/gc_vibration_device.h"

namespace Service::HID {

NpadGcVibrationDevice::NpadGcVibrationDevice() {}

Result NpadGcVibrationDevice::IncrementRefCounter() {
    if (ref_counter == 0 && is_mounted) {
        f32 volume = 1.0f;
        const auto result = vibration_handler->GetVibrationVolume(volume);
        if (result.IsSuccess()) {
            // TODO: SendVibrationGcErmCommand
        }
    }
    ref_counter++;
    return ResultSuccess;
}

Result NpadGcVibrationDevice::DecrementRefCounter() {
    if (ref_counter == 1 && !is_mounted) {
        f32 volume = 1.0f;
        const auto result = vibration_handler->GetVibrationVolume(volume);
        if (result.IsSuccess()) {
            // TODO: SendVibrationGcErmCommand
        }
    }

    if (ref_counter > 0) {
        ref_counter--;
    }

    return ResultSuccess;
}

Result NpadGcVibrationDevice::SendVibrationGcErmCommand(Core::HID::VibrationGcErmCommand command) {
    if (!is_mounted) {
        return ResultSuccess;
    }
    f32 volume = 1.0f;
    const auto result = vibration_handler->GetVibrationVolume(volume);
    if (result.IsError()) {
        return result;
    }
    if (volume == 0.0) {
        command = Core::HID::VibrationGcErmCommand::Stop;
    } else {
        if (command > Core::HID::VibrationGcErmCommand::StopHard) {
            // Abort
            return ResultSuccess;
        }
    }
    // TODO: SendVibrationGcErmCommand
    return ResultSuccess;
}

Result NpadGcVibrationDevice::GetActualVibrationGcErmCommand(
    Core::HID::VibrationGcErmCommand& out_command) {
    if (!is_mounted) {
        out_command = Core::HID::VibrationGcErmCommand::Stop;
        return ResultSuccess;
    }

    f32 volume = 1.0f;
    const auto result = vibration_handler->GetVibrationVolume(volume);
    if (result.IsError()) {
        return result;
    }
    if (volume == 0.0f) {
        out_command = Core::HID::VibrationGcErmCommand::Stop;
        return ResultSuccess;
    }

    // TODO: GetActualVibrationGcErmCommand
    return ResultSuccess;
}

Result NpadGcVibrationDevice::SendVibrationNotificationPattern(
    Core::HID::VibrationGcErmCommand command) {
    if (!is_mounted) {
        return ResultSuccess;
    }

    f32 volume = 1.0f;
    const auto result = vibration_handler->GetVibrationVolume(volume);
    if (result.IsError()) {
        return result;
    }
    if (volume <= 0.0f) {
        command = Core::HID::VibrationGcErmCommand::Stop;
    }
    if (command > Core::HID::VibrationGcErmCommand::StopHard) {
        // Abort
        return ResultSuccess;
    }

    // TODO: SendVibrationNotificationPattern
    return ResultSuccess;
}

} // namespace Service::HID
