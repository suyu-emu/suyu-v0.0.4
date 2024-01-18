// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input_common/drivers/android.h"

namespace InputCommon {

Android::Android(std::string input_engine_) : InputEngine(std::move(input_engine_)) {}

void Android::RegisterController(std::size_t controller_number) {
    PreSetController(GetIdentifier(controller_number));
}

void Android::SetButtonState(std::size_t controller_number, int button_id, bool value) {
    const auto identifier = GetIdentifier(controller_number);
    SetButton(identifier, button_id, value);
}

void Android::SetAxisState(std::size_t controller_number, int axis_id, float value) {
    const auto identifier = GetIdentifier(controller_number);
    SetAxis(identifier, axis_id, value);
}

void Android::SetMotionState(std::size_t controller_number, u64 delta_timestamp, float gyro_x,
                             float gyro_y, float gyro_z, float accel_x, float accel_y,
                             float accel_z) {
    const auto identifier = GetIdentifier(controller_number);
    const BasicMotion motion_data{
        .gyro_x = gyro_x,
        .gyro_y = gyro_y,
        .gyro_z = gyro_z,
        .accel_x = accel_x,
        .accel_y = accel_y,
        .accel_z = accel_z,
        .delta_timestamp = delta_timestamp,
    };
    SetMotion(identifier, 0, motion_data);
}

PadIdentifier Android::GetIdentifier(std::size_t controller_number) const {
    return {
        .guid = Common::UUID{},
        .port = controller_number,
        .pad = 0,
    };
}

} // namespace InputCommon
