// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Common {
struct PowerStatus {
    int percentage = -1;
    bool charging = false;
    bool has_battery = true;
};

PowerStatus GetPowerStatus();
} // namespace Common