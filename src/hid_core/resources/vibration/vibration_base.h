// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/result.h"

namespace Service::HID {
class NpadVibration;

/// Handles Npad request from HID interfaces
class NpadVibrationBase {
public:
    explicit NpadVibrationBase();

    virtual Result IncrementRefCounter();
    virtual Result DecrementRefCounter();

    bool IsVibrationMounted() const;

protected:
    u64 xcd_handle{};
    s32 ref_counter{};
    bool is_mounted{};
    NpadVibration* vibration_handler{nullptr};
};
} // namespace Service::HID
