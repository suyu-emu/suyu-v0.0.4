// SPDX-FileCopyrightText: Copyright 2026 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include "core/internal_network/wifi_scanner.h"

namespace Network {
std::vector<Network::ScanData> ScanWifiNetworks(std::chrono::milliseconds deadline) {
    return {}; // disabled, pretend no results
}
} // namespace Network
