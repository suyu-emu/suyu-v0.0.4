// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

#include "common/bit_cast.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/polyfill_ranges.h"
#include "common/settings.h"
#include "common/string_util.h"
#include "core/internal_network/emu_net_state.h"
#include "core/internal_network/network_interface.h"

#ifdef _WIN32
#include <iphlpapi.h>
#else
#include <cerrno>
#include <ifaddrs.h>
#include <net/if.h>
#endif

namespace Network {

#ifdef _WIN32

std::vector<Network::NetworkInterface> GetAvailableNetworkInterfaces() {

    ULONG buf_size = 0;
    if (GetAdaptersAddresses(
            AF_INET, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS,
            nullptr, nullptr, &buf_size) != ERROR_BUFFER_OVERFLOW) {
        LOG_ERROR(Network, "GetAdaptersAddresses(overrun probe) failed");
        return {};
    }

    std::vector<u8> buffer(buf_size, 0);
    auto* addrs = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());

    if (GetAdaptersAddresses(
            AF_INET, GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_GATEWAYS,
            nullptr, addrs, &buf_size) != NO_ERROR) {
        LOG_ERROR(Network, "GetAdaptersAddresses(data) failed");
        return {};
    }

    std::vector<Network::NetworkInterface> result;

    for (auto* a = addrs; a; a = a->Next) {

        if (a->OperStatus != IfOperStatusUp || !a->FirstUnicastAddress ||
            !a->FirstUnicastAddress->Address.lpSockaddr)
            continue;

        const in_addr ip =
            reinterpret_cast<sockaddr_in*>(a->FirstUnicastAddress->Address.lpSockaddr)->sin_addr;

        ULONG mask_raw = 0;
        if (ConvertLengthToIpv4Mask(a->FirstUnicastAddress->OnLinkPrefixLength, &mask_raw) !=
            NO_ERROR)
            continue;

        in_addr mask{.S_un{.S_addr{mask_raw}}};

        in_addr gw{.S_un{.S_addr{0}}};
        if (a->FirstGatewayAddress && a->FirstGatewayAddress->Address.lpSockaddr)
            gw = reinterpret_cast<sockaddr_in*>(a->FirstGatewayAddress->Address.lpSockaddr)
                     ->sin_addr;

        HostAdapterKind kind = HostAdapterKind::Ethernet;
        switch (a->IfType) {
        case IF_TYPE_IEEE80211: // 802.11 Wi-Fi
            kind = HostAdapterKind::Wifi;
            break;
        default:
            kind = HostAdapterKind::Ethernet;
            break;
        }

        result.emplace_back(Network::NetworkInterface{
            .name = Common::UTF16ToUTF8(std::wstring{a->FriendlyName}),
            .ip_address = ip,
            .subnet_mask = mask,
            .gateway = gw,
            .kind = kind
        });
    }

    return result;
}

#else

std::vector<Network::NetworkInterface> GetAvailableNetworkInterfaces() {
    struct ifaddrs* ifaddr = nullptr;

    if (getifaddrs(&ifaddr) != 0) {
        LOG_ERROR(Network, "Failed to get network interfaces with getifaddrs: {}",
                  std::strerror(errno));
        return {};
    }

    std::vector<Network::NetworkInterface> result;

    for (auto ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr || ifa->ifa_netmask == nullptr) {
            continue;
        }

        if (ifa->ifa_addr->sa_family != AF_INET) {
            continue;
        }

        if ((ifa->ifa_flags & IFF_UP) == 0 || (ifa->ifa_flags & IFF_LOOPBACK) != 0) {
            continue;
        }

#ifdef ANDROID
        // On Android, we can't reliably get gateway info from /proc/net/route
        // Just use 0 as the gateway address
        result.emplace_back(Network::NetworkInterface{
                .name{ifa->ifa_name},
                .ip_address{Common::BitCast<struct sockaddr_in>(*ifa->ifa_addr).sin_addr},
                .subnet_mask{Common::BitCast<struct sockaddr_in>(*ifa->ifa_netmask).sin_addr},
                .gateway{in_addr{.s_addr = 0}}
        });
#else
        u32 gateway{};

        std::ifstream file{"/proc/net/route"};
        if (!file.is_open()) {
            LOG_ERROR(Network, "Failed to open \"/proc/net/route\"");

            // Solaris defines s_addr as a macro, can't use special C++ shenanigans here
            in_addr gateway_0;
            gateway_0.s_addr = gateway;
            result.emplace_back(Network::NetworkInterface{
                .name = ifa->ifa_name,
                .ip_address = Common::BitCast<struct sockaddr_in>(*ifa->ifa_addr).sin_addr,
                .subnet_mask = Common::BitCast<struct sockaddr_in>(*ifa->ifa_netmask).sin_addr,
                .gateway = gateway_0
            });
            continue;
        }

        // ignore header
        file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

        bool gateway_found = false;

        for (std::string line; std::getline(file, line);) {
            std::istringstream iss{line};

            std::string iface_name;
            iss >> iface_name;
            if (iface_name != ifa->ifa_name) {
                continue;
            }

            iss >> std::hex;

            u32 dest{};
            iss >> dest;
            if (dest != 0) {
                // not the default route
                continue;
            }

            iss >> gateway;

            u16 flags{};
            iss >> flags;

            // flag RTF_GATEWAY (defined in <linux/route.h>)
            if ((flags & 0x2) == 0) {
                continue;
            }

            gateway_found = true;
            break;
        }

        if (!gateway_found) {
            gateway = 0;
        }

        in_addr gateway_0;
        gateway_0.s_addr = gateway;
        result.emplace_back(Network::NetworkInterface{
            .name = ifa->ifa_name,
            .ip_address = Common::BitCast<struct sockaddr_in>(*ifa->ifa_addr).sin_addr,
            .subnet_mask = Common::BitCast<struct sockaddr_in>(*ifa->ifa_netmask).sin_addr,
            .gateway = gateway_0
        });
#endif // ANDROID
    }

    freeifaddrs(ifaddr);
    return result;
}

#endif // _WIN32

std::optional<Network::NetworkInterface> GetSelectedNetworkInterface() {

    const auto& selected_network_interface = Settings::values.network_interface.GetValue();
    const auto network_interfaces = Network::GetAvailableNetworkInterfaces();
    if (network_interfaces.empty()) {
        LOG_ERROR(Network, "GetAvailableNetworkInterfaces returned no interfaces");
        return std::nullopt;
    }

    #ifdef __ANDROID__
        if (selected_network_interface.empty()) {
            return network_interfaces[0];
        }
    #endif

    const auto res =
        std::ranges::find_if(network_interfaces, [&selected_network_interface](const auto& iface) {
            return iface.name == selected_network_interface;
        });

    if (res == network_interfaces.end()) {
        // Only print the error once to avoid log spam
        static bool print_error = true;
        if (print_error) {
            LOG_ERROR(Network, "Couldn't find selected interface \"{}\"",
                      selected_network_interface);
            print_error = false;
        }

        return std::nullopt;
    }

    return *res;
}

void SelectFirstNetworkInterface() {
    const auto network_interfaces = Network::GetAvailableNetworkInterfaces();

    if (network_interfaces.empty()) {
        return;
    }

    Settings::values.network_interface.SetValue(network_interfaces[0].name);
}

} // namespace Network
