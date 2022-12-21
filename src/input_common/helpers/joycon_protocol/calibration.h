// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// Based on dkms-hid-nintendo implementation, CTCaer joycon toolkit and dekuNukem reverse
// engineering https://github.com/nicman23/dkms-hid-nintendo/blob/master/src/hid-nintendo.c
// https://github.com/CTCaer/jc_toolkit
// https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering

#pragma once

#include <vector>

#include "input_common/helpers/joycon_protocol/common_protocol.h"

namespace InputCommon::Joycon {
enum class DriverResult;
struct JoyStickCalibration;
struct IMUCalibration;
struct JoyconHandle;
} // namespace InputCommon::Joycon

namespace InputCommon::Joycon {

/// Driver functions related to retrieving calibration data from the device
class CalibrationProtocol final : private JoyconCommonProtocol {
public:
    CalibrationProtocol(std::shared_ptr<JoyconHandle> handle);

    /**
     * Sends a request to obtain the left stick calibration from memory
     * @param is_factory_calibration if true factory values will be returned
     * @returns JoyStickCalibration of the left joystick
     */
    DriverResult GetLeftJoyStickCalibration(JoyStickCalibration& calibration);

    /**
     * Sends a request to obtain the right stick calibration from memory
     * @param is_factory_calibration if true factory values will be returned
     * @returns JoyStickCalibration of the right joystick
     */
    DriverResult GetRightJoyStickCalibration(JoyStickCalibration& calibration);

    /**
     * Sends a request to obtain the motion calibration from memory
     * @returns ImuCalibration of the motion sensor
     */
    DriverResult GetImuCalibration(MotionCalibration& calibration);

private:
    void ValidateCalibration(JoyStickCalibration& calibration);
    void ValidateCalibration(MotionCalibration& calibration);
};

} // namespace InputCommon::Joycon
