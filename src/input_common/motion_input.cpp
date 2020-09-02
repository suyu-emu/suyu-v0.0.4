// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included

#include "common/math_util.h"
#include "input_common/motion_input.h"

namespace InputCommon {

MotionInput::MotionInput(f32 new_kp, f32 new_ki, f32 new_kd)
    : kp(new_kp), ki(new_ki), kd(new_kd), quat{{0, 0, -1}, 0} {}

void MotionInput::SetAcceleration(const Common::Vec3f& acceleration) {
    accel = acceleration;
}

void MotionInput::SetGyroscope(const Common::Vec3f& gyroscope) {
    gyro = gyroscope - gyro_drift;
    if (gyro.Length2() < gyro_threshold) {
        gyro = {};
    }
}

void MotionInput::SetQuaternion(const Common::Quaternion<f32>& quaternion) {
    quat = quaternion;
}

void MotionInput::SetGyroDrift(const Common::Vec3f& drift) {
    gyro_drift = drift;
}

void MotionInput::SetGyroThreshold(f32 threshold) {
    gyro_threshold = threshold;
}

void MotionInput::EnableReset(bool reset) {
    reset_enabled = reset;
}

void MotionInput::ResetRotations() {
    rotations = {};
}

bool MotionInput::IsMoving(f32 sensitivity) const {
    return gyro.Length() >= sensitivity || accel.Length() <= 0.9f || accel.Length() >= 1.1f;
}

bool MotionInput::IsCalibrated(f32 sensitivity) const {
    return real_error.Length() < sensitivity;
}

void MotionInput::UpdateRotation(u64 elapsed_time) {
    const f32 sample_period = elapsed_time / 1000000.0f;
    if (sample_period > 0.1f) {
        return;
    }
    rotations += gyro * sample_period;
}

void MotionInput::UpdateOrientation(u64 elapsed_time) {
    if (!IsCalibrated(0.1f)) {
        ResetOrientation();
    }
    // Short name local variable for readability
    f32 q1 = quat.w;
    f32 q2 = quat.xyz[0];
    f32 q3 = quat.xyz[1];
    f32 q4 = quat.xyz[2];
    const f32 sample_period = elapsed_time / 1000000.0f;

    // ignore invalid elapsed time
    if (sample_period > 0.1f) {
        return;
    }

    const auto normal_accel = accel.Normalized();
    auto rad_gyro = gyro * Common::PI * 2;
    const f32 swap = rad_gyro.x;
    rad_gyro.x = rad_gyro.y;
    rad_gyro.y = -swap;
    rad_gyro.z = -rad_gyro.z;

    // Ignore drift correction if acceleration is not reliable
    if (accel.Length() >= 0.75f && accel.Length() <= 1.25f) {
        const f32 ax = -normal_accel.x;
        const f32 ay = normal_accel.y;
        const f32 az = -normal_accel.z;

        // Estimated direction of gravity
        const f32 vx = 2.0f * (q2 * q4 - q1 * q3);
        const f32 vy = 2.0f * (q1 * q2 + q3 * q4);
        const f32 vz = q1 * q1 - q2 * q2 - q3 * q3 + q4 * q4;

        // Error is cross product between estimated direction and measured direction of gravity
        const Common::Vec3f new_real_error = {az * vx - ax * vz, ay * vz - az * vy,
                                              ax * vy - ay * vx};

        derivative_error = new_real_error - real_error;
        real_error = new_real_error;

        // Prevent integral windup
        if (ki != 0.0f && !IsCalibrated(0.05f)) {
            integral_error += real_error;
        } else {
            integral_error = {};
        }

        // Apply feedback terms
        rad_gyro += kp * real_error;
        rad_gyro += ki * integral_error;
        rad_gyro += kd * derivative_error;
    }

    const f32 gx = rad_gyro.y;
    const f32 gy = rad_gyro.x;
    const f32 gz = rad_gyro.z;

    // Integrate rate of change of quaternion
    const f32 pa = q2;
    const f32 pb = q3;
    const f32 pc = q4;
    q1 = q1 + (-q2 * gx - q3 * gy - q4 * gz) * (0.5f * sample_period);
    q2 = pa + (q1 * gx + pb * gz - pc * gy) * (0.5f * sample_period);
    q3 = pb + (q1 * gy - pa * gz + pc * gx) * (0.5f * sample_period);
    q4 = pc + (q1 * gz + pa * gy - pb * gx) * (0.5f * sample_period);

    quat.w = q1;
    quat.xyz[0] = q2;
    quat.xyz[1] = q3;
    quat.xyz[2] = q4;
    quat = quat.Normalized();
}

std::array<Common::Vec3f, 3> MotionInput::GetOrientation() const {
    const Common::Quaternion<float> quad{
        .xyz = {-quat.xyz[1], -quat.xyz[0], -quat.w},
        .w = -quat.xyz[2],
    };
    const std::array<float, 16> matrix4x4 = quad.ToMatrix();

    return {Common::Vec3f(matrix4x4[0], matrix4x4[1], -matrix4x4[2]),
            Common::Vec3f(matrix4x4[4], matrix4x4[5], -matrix4x4[6]),
            Common::Vec3f(-matrix4x4[8], -matrix4x4[9], matrix4x4[10])};
}

Common::Vec3f MotionInput::GetAcceleration() const {
    return accel;
}

Common::Vec3f MotionInput::GetGyroscope() const {
    return gyro;
}

Common::Quaternion<f32> MotionInput::GetQuaternion() const {
    return quat;
}

Common::Vec3f MotionInput::GetRotations() const {
    return rotations;
}

void MotionInput::ResetOrientation() {
    if (!reset_enabled) {
        return;
    }
    if (!IsMoving(0.5f) && accel.z <= -0.9f) {
        ++reset_counter;
        if (reset_counter > 900) {
            // TODO: calculate quaternion from gravity vector
            quat.w = 0;
            quat.xyz[0] = 0;
            quat.xyz[1] = 0;
            quat.xyz[2] = -1;
            integral_error = {};
            reset_counter = 0;
        }
    } else {
        reset_counter = 0;
    }
}
} // namespace InputCommon
