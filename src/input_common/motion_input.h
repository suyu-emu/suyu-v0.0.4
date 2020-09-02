// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#pragma once

#include "common/common_types.h"
#include "common/quaternion.h"
#include "common/vector_math.h"

namespace InputCommon {

class MotionInput {
public:
    MotionInput(f32 new_kp, f32 new_ki, f32 new_kd);

    MotionInput(const MotionInput&) = default;
    MotionInput& operator=(const MotionInput&) = default;

    MotionInput(MotionInput&&) = default;
    MotionInput& operator=(MotionInput&&) = default;

    void SetAcceleration(const Common::Vec3f& acceleration);
    void SetGyroscope(const Common::Vec3f& acceleration);
    void SetQuaternion(const Common::Quaternion<f32>& quaternion);
    void SetGyroDrift(const Common::Vec3f& drift);
    void SetGyroThreshold(f32 threshold);

    void EnableReset(bool reset);
    void ResetRotations();

    void UpdateRotation(u64 elapsed_time);
    void UpdateOrientation(u64 elapsed_time);

    std::array<Common::Vec3f, 3> GetOrientation() const;
    Common::Vec3f GetAcceleration() const;
    Common::Vec3f GetGyroscope() const;
    Common::Vec3f GetRotations() const;
    Common::Quaternion<f32> GetQuaternion() const;

    bool IsMoving(f32 sensitivity) const;
    bool IsCalibrated(f32 sensitivity) const;

private:
    void ResetOrientation();

    // PID constants
    const f32 kp;
    const f32 ki;
    const f32 kd;

    // PID errors
    Common::Vec3f real_error;
    Common::Vec3f integral_error;
    Common::Vec3f derivative_error;

    Common::Quaternion<f32> quat;
    Common::Vec3f rotations;
    Common::Vec3f accel;
    Common::Vec3f gyro;
    Common::Vec3f gyro_drift;

    f32 gyro_threshold = 0.0f;
    u32 reset_counter = 0;
    bool reset_enabled = true;
};

} // namespace InputCommon
