#include "input_common/motion_input.h"

namespace InputCommon {

MotionInput::MotionInput(f32 new_kp, f32 new_ki, f32 new_kd) : kp(new_kp), ki(new_ki), kd(new_kd) {
    accel = {};
    gyro = {};
    gyro_drift = {};
    gyro_threshold = 0;
    rotations = {};

    quat.w = 0;
    quat.xyz[0] = 0;
    quat.xyz[1] = 0;
    quat.xyz[2] = -1;

    real_error = {};
    integral_error = {};
    derivative_error = {};

    reset_counter = 0;
    reset_enabled = true;
}

void MotionInput::SetAcceleration(Common::Vec3f acceleration) {
    accel = acceleration;
}

void MotionInput::SetGyroscope(Common::Vec3f gyroscope) {
    gyro = gyroscope - gyro_drift;
    if (gyro.Length2() < gyro_threshold) {
        gyro = {};
    }
}

void MotionInput::SetQuaternion(Common::Quaternion<f32> quaternion) {
    quat = quaternion;
}

void MotionInput::SetGyroDrift(Common::Vec3f drift) {
    drift = gyro_drift;
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

bool MotionInput::IsMoving(f32 sensitivity) {
    return gyro.Length2() >= sensitivity || accel.Length() <= 0.9f || accel.Length() >= 1.1f;
}

bool MotionInput::IsCalibrated(f32 sensitivity) {
    return real_error.Length() > sensitivity;
}

void MotionInput::UpdateRotation(u64 elapsed_time) {
    rotations += gyro * elapsed_time;
}

void MotionInput::UpdateOrientation(u64 elapsed_time) {
    // Short name local variable for readability
    f32 q1 = quat.w, q2 = quat.xyz[0], q3 = quat.xyz[1], q4 = quat.xyz[2];
    f32 sample_period = elapsed_time / 1000000.0f;

    auto normal_accel = accel.Normalized();
    auto rad_gyro = gyro * 3.1415926535f;
    rad_gyro.z = -rad_gyro.z;

    // Ignore drift correction if acceleration is not present
    if (normal_accel.Length() == 1.0f) {
        f32 ax = -normal_accel.x;
        f32 ay = normal_accel.y;
        f32 az = -normal_accel.z;
        f32 vx, vy, vz;
        Common::Vec3f new_real_error;

        // Estimated direction of gravity
        vx = 2.0f * (q2 * q4 - q1 * q3);
        vy = 2.0f * (q1 * q2 + q3 * q4);
        vz = q1 * q1 - q2 * q2 - q3 * q3 + q4 * q4;

        // Error is cross product between estimated direction and measured direction of gravity
        new_real_error.x = ay * vz - az * vy;
        new_real_error.y = az * vx - ax * vz;
        new_real_error.x = ax * vy - ay * vx;

        derivative_error = new_real_error - real_error;
        real_error = new_real_error;

        // Prevent integral windup
        if (ki != 0.0f) {
            integral_error += real_error;
        } else {
            integral_error = {};
        }

        // Apply feedback terms
        rad_gyro += kp * real_error;
        rad_gyro += ki * integral_error;
        rad_gyro += kd * derivative_error;
    }

    f32 gx = rad_gyro.y;
    f32 gy = rad_gyro.x;
    f32 gz = rad_gyro.z;

    // Integrate rate of change of quaternion
    f32 pa, pb, pc;
    pa = q2;
    pb = q3;
    pc = q4;
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

std::array<Common::Vec3f, 3> MotionInput::GetOrientation() {
    std::array<Common::Vec3f, 3> orientation = {};
    Common::Quaternion<float> quad;

    quad.w = -quat.xyz[2];
    quad.xyz[0] = -quat.xyz[1];
    quad.xyz[1] = -quat.xyz[0];
    quad.xyz[2] = -quat.w;

    std::array<float, 16> matrix4x4 = quad.ToMatrix();

    orientation[0] = Common::Vec3f(matrix4x4[0], matrix4x4[1], matrix4x4[2]);
    orientation[1] = Common::Vec3f(matrix4x4[4], matrix4x4[5], matrix4x4[6]);
    orientation[2] = Common::Vec3f(matrix4x4[8], matrix4x4[9], matrix4x4[10]);

    return orientation;
}

Common::Vec3f MotionInput::GetAcceleration() {
    return accel;
}

Common::Vec3f MotionInput::GetGyroscope() {
    return gyro;
}

Common::Quaternion<f32> MotionInput::GetQuaternion() {
    return quat;
}

Common::Vec3f MotionInput::GetRotations() {
    return rotations;
}

void MotionInput::resetOrientation() {
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