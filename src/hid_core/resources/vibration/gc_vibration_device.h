// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <mutex>

#include "common/common_types.h"
#include "core/hle/result.h"
#include "hid_core/hid_types.h"
#include "hid_core/resources/npad/npad_types.h"
#include "hid_core/resources/vibration/vibration_base.h"

namespace Service::HID {
class NpadVibration;

/// Handles Npad request from HID interfaces
class NpadGcVibrationDevice final : public NpadVibrationBase {
public:
    explicit NpadGcVibrationDevice();

    Result IncrementRefCounter() override;
    Result DecrementRefCounter() override;

    Result SendVibrationGcErmCommand(Core::HID::VibrationGcErmCommand command);

    Result GetActualVibrationGcErmCommand(Core::HID::VibrationGcErmCommand& out_command);
    Result SendVibrationNotificationPattern(Core::HID::VibrationGcErmCommand command);
};
} // namespace Service::HID
