// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "input_common/input_engine.h"

namespace InputCommon {

/**
 * A virtual controller that is always assigned to the game input
 */
class Android final : public InputEngine {
public:
    explicit Android(std::string input_engine_);

    /**
     * Registers controller number to accept new inputs
     * @param controller_number the controller number that will take this action
     */
    void RegisterController(std::size_t controller_number);

    /**
     * Sets the status of all buttons bound with the key to pressed
     * @param controller_number the controller number that will take this action
     * @param button_id the id of the button
     * @param value indicates if the button is pressed or not
     */
    void SetButtonState(std::size_t controller_number, int button_id, bool value);

    /**
     * Sets the status of a analog input to a specific player index
     * @param controller_number the controller number that will take this action
     * @param axis_id the id of the axis to move
     * @param value the analog position of the axis
     */
    void SetAxisState(std::size_t controller_number, int axis_id, float value);

    /**
     * Sets the status of the motion sensor to a specific player index
     * @param controller_number the controller number that will take this action
     * @param delta_timestamp time passed since last reading
     * @param gyro_x,gyro_y,gyro_z the gyro sensor readings
     * @param accel_x,accel_y,accel_z the accelerometer reading
     */
    void SetMotionState(std::size_t controller_number, u64 delta_timestamp, float gyro_x,
                        float gyro_y, float gyro_z, float accel_x, float accel_y, float accel_z);

private:
    /// Returns the correct identifier corresponding to the player index
    PadIdentifier GetIdentifier(std::size_t controller_number) const;
};

} // namespace InputCommon
