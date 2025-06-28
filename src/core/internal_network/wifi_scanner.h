// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <chrono>
#include <vector>

#include "core/core.h"

namespace Network {

struct ScanData {
    u8 ssid_len{};
    char ssid[0x21]{};
    u8 quality{};
    u32 flags{};
};
static_assert(sizeof(ScanData) <= 0x2C, "ScanData layout changed – update conversions!");

std::vector<Network::ScanData> ScanWifiNetworks(std::chrono::milliseconds deadline);
}
