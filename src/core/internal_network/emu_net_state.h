// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <atomic>
#include <common/socket_types.h>
#include <mutex>

namespace Network {

struct EmuNetState {

    static EmuNetState& Get();

    EmuNetState(const EmuNetState&) = delete;
    EmuNetState& operator=(const EmuNetState&) = delete;
    EmuNetState(EmuNetState&&) = delete;
    EmuNetState& operator=(EmuNetState&&) = delete;

    std::atomic<bool> wifi_enabled{true};
    std::atomic<bool> ethernet_enabled{true};

    std::mutex mtx;
    bool connected = false;
    bool via_wifi = false;
    char ssid[20] = {};
    u8 bars = 0;
    bool secure = false;
    IPv4Address ip{}, mask{}, gw{};

private:
    EmuNetState() = default;
};

void RefreshFromHost();
u8 QualityToBars(u8 quality);

} // namespace Network
