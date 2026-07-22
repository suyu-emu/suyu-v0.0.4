// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <netinet/in.h>
#endif

namespace Network {

enum class HostAdapterKind { Wifi, Ethernet };

struct NetworkInterface {
    std::string name;
    struct in_addr ip_address;
    struct in_addr subnet_mask;
    struct in_addr gateway;
    HostAdapterKind kind{HostAdapterKind::Ethernet};
};

std::vector<NetworkInterface> GetAvailableNetworkInterfaces();
std::optional<NetworkInterface> GetSelectedNetworkInterface();
void SelectFirstNetworkInterface();

struct HostAdapter {
    in_addr ip_address;
    in_addr subnet_mask;
    in_addr gateway;
    std::string name;
    HostAdapterKind kind;
};

} // namespace Network
