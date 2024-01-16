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
class NpadN64VibrationDevice final : public NpadVibrationBase {
public:
    explicit NpadN64VibrationDevice();

    Result IncrementRefCounter() override;
    Result DecrementRefCounter() override;

    Result SendValueInBool(bool is_vibrating);
    Result SendVibrationNotificationPattern(u32 pattern);
};
} // namespace Service::HID
