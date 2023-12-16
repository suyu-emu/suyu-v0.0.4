// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>

#include "common/common_types.h"

namespace Service::Set {
struct DeviceSettings {
    std::array<u8, 0x10> reserved_000;

    // nn::settings::BatteryLot
    std::array<u8, 0x18> ptm_battery_lot;
    // nn::settings::system::PtmFuelGaugeParameter
    std::array<u8, 0x18> ptm_fuel_gauge_parameter;
    u8 ptm_battery_version;
    // nn::settings::system::PtmCycleCountReliability
    u32 ptm_cycle_count_reliability;

    std::array<u8, 0x48> reserved_048;

    // nn::settings::system::AnalogStickUserCalibration L
    std::array<u8, 0x10> analog_user_stick_calibration_l;
    // nn::settings::system::AnalogStickUserCalibration R
    std::array<u8, 0x10> analog_user_stick_calibration_r;

    std::array<u8, 0x20> reserved_0B0;

    // nn::settings::system::ConsoleSixAxisSensorAccelerationBias
    std::array<u8, 0xC> console_six_axis_sensor_acceleration_bias;
    // nn::settings::system::ConsoleSixAxisSensorAngularVelocityBias
    std::array<u8, 0xC> console_six_axis_sensor_angular_velocity_bias;
    // nn::settings::system::ConsoleSixAxisSensorAccelerationGain
    std::array<u8, 0x24> console_six_axis_sensor_acceleration_gain;
    // nn::settings::system::ConsoleSixAxisSensorAngularVelocityGain
    std::array<u8, 0x24> console_six_axis_sensor_angular_velocity_gain;
    // nn::settings::system::ConsoleSixAxisSensorAngularVelocityTimeBias
    std::array<u8, 0xC> console_six_axis_sensor_angular_velocity_time_bias;
    // nn::settings::system::ConsoleSixAxisSensorAngularAcceleration
    std::array<u8, 0x24> console_six_axis_sensor_angular_acceleration;
};
static_assert(offsetof(DeviceSettings, ptm_battery_lot) == 0x10);
static_assert(offsetof(DeviceSettings, ptm_cycle_count_reliability) == 0x44);
static_assert(offsetof(DeviceSettings, analog_user_stick_calibration_l) == 0x90);
static_assert(offsetof(DeviceSettings, console_six_axis_sensor_acceleration_bias) == 0xD0);
static_assert(offsetof(DeviceSettings, console_six_axis_sensor_angular_acceleration) == 0x13C);
static_assert(sizeof(DeviceSettings) == 0x160, "DeviceSettings has the wrong size!");

DeviceSettings DefaultDeviceSettings();

} // namespace Service::Set
