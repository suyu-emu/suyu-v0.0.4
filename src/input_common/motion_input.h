// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/quaternion.h"
#include "common/vector_math.h"

namespace InputCommon {

class MotionInput {
public:
    MotionInput(f32 new_kp, f32 new_ki, f32 new_kd);

    void SetAcceleration(Common::Vec3f acceleration);
    void SetGyroscope(Common::Vec3f acceleration);
    void SetQuaternion(Common::Quaternion<f32> quaternion);
    void SetGyroDrift(Common::Vec3f drift);
    void SetGyroThreshold(f32 threshold);

    void EnableReset(bool reset);
    void ResetRotations();

    void UpdateRotation(u64 elapsed_time);
    void UpdateOrientation(u64 elapsed_time);

    std::array<Common::Vec3f, 3> GetOrientation();
    Common::Vec3f GetAcceleration();
    Common::Vec3f GetGyroscope();
    Common::Vec3f GetRotations();
    Common::Quaternion<f32> GetQuaternion();

    bool IsMoving(f32 sensitivity);
    bool IsCalibrated(f32 sensitivity);

    // PID constants
    const f32 kp;
    const f32 ki;
    const f32 kd;

private:
    void resetOrientation();

    // PID errors
    Common::Vec3f real_error;
    Common::Vec3f integral_error;
    Common::Vec3f derivative_error;

    Common::Quaternion<f32> quat;
    Common::Vec3f rotations;
    Common::Vec3f accel;
    Common::Vec3f gyro;
    Common::Vec3f gyro_drift;

    f32 gyro_threshold;
    f32 reset_counter;
    bool reset_enabled;
};

} // namespace InputCommon
