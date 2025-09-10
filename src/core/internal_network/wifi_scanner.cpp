// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <chrono>
#include <cstring>
#include <thread>
#include <vector>

#include "common/logging/log.h"
#include "core/internal_network/wifi_scanner.h"

using namespace std::chrono_literals;

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wlanapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "wlanapi.lib")
#endif
#endif

namespace Network {
#ifdef ENABLE_WIFI_SCAN
#ifdef _WIN32
static u8 QualityToPercent(DWORD q) {
    return static_cast<u8>(q);
}

static std::vector<Network::ScanData> ScanWifiWin(std::chrono::milliseconds deadline) {
    std::vector<Network::ScanData> out;

    HANDLE hClient{};
    DWORD ver{};
    if (WlanOpenHandle(2, nullptr, &ver, &hClient) != ERROR_SUCCESS)
        return out;

    PWLAN_INTERFACE_INFO_LIST ifs{};
    if (WlanEnumInterfaces(hClient, nullptr, &ifs) != ERROR_SUCCESS || ifs->dwNumberOfItems == 0) {
        WlanCloseHandle(hClient, nullptr);
        return out;
    }

    // fire a scan on every adapter
    for (DWORD i = 0; i < ifs->dwNumberOfItems; ++i)
        WlanScan(hClient, &ifs->InterfaceInfo[i].InterfaceGuid, nullptr, nullptr, nullptr);

    const auto start = std::chrono::steady_clock::now();
    bool have = false;

    while (!have && std::chrono::steady_clock::now() - start < deadline) {
        std::this_thread::sleep_for(100ms);
        out.clear();

        for (DWORD i = 0; i < ifs->dwNumberOfItems; ++i) {
            PWLAN_AVAILABLE_NETWORK_LIST list{};
            if (WlanGetAvailableNetworkList(hClient, &ifs->InterfaceInfo[i].InterfaceGuid,
                                            WLAN_AVAILABLE_NETWORK_INCLUDE_ALL_ADHOC_PROFILES,
                                            nullptr, &list) != ERROR_SUCCESS)
                continue;

            for (DWORD n = 0; n < list->dwNumberOfItems; ++n) {
                const auto& nw = list->Network[n];
                Network::ScanData sd{};
                sd.ssid_len = static_cast<u8>(nw.dot11Ssid.uSSIDLength);
                std::memcpy(sd.ssid, nw.dot11Ssid.ucSSID, sd.ssid_len);
                sd.quality = QualityToPercent(nw.wlanSignalQuality);
                if (nw.bNetworkConnectable)
                    sd.flags |= 1;
                if (nw.bSecurityEnabled)
                    sd.flags |= 2;
                out.emplace_back(sd);

                char tmp[0x22]{};
                std::memcpy(tmp, sd.ssid, sd.ssid_len);
                LOG_INFO(Network, "[WifiScan] +%u \"%s\" quality=%u flags=0x%X", sd.ssid_len, tmp,
                         sd.quality, sd.flags);
            }
            WlanFreeMemory(list);
        }
        have = !out.empty();
    }

    WlanFreeMemory(ifs);
    WlanCloseHandle(hClient, nullptr);
    return out;
}
#endif /* _WIN32 */

#if defined(__linux__) && !defined(_WIN32) && !defined(ANDROID)
#include <iwlib.h>

static u8 QualityToPercent(const iwrange& r, const wireless_scan* ws) {
    const iw_quality qual = ws->stats.qual;
    const int lvl = qual.level;
    const int max = r.max_qual.level ? r.max_qual.level : 100;
    return static_cast<u8>(std::clamp(100 * lvl / max, 0, 100));
}

static int wifi_callback(int skfd, char* ifname, char* args[], int count)
{
    iwrange range;

    int res = iw_get_range_info(skfd, ifname, &range);

    LOG_INFO(Network, "ifname {} returned {} on iw_get_range_info", ifname, res);

    if (res >= 0) {
        strncpy(args[0], ifname, IFNAMSIZ - 1);
        args[0][IFNAMSIZ - 1] = 0;

        return 1;
    }

    return 0;
}

// TODO(crueter, Maufeat): Check if driver supports wireless extensions, fallback to nl80211 if not
static std::vector<Network::ScanData> ScanWifiLinux(std::chrono::milliseconds deadline) {
    std::vector<Network::ScanData> out;
    int sock = iw_sockets_open();
    if (sock < 0) {
        LOG_ERROR(Network, "iw_sockets_open() failed");
        return out;
    }

    char ifname[IFNAMSIZ] = {0};
    char *args[1] = {ifname};

    iw_enum_devices(sock, &wifi_callback, args, 0);

    if (strlen(ifname) == 0) {
        LOG_WARNING(Network, "No wireless interface found");
        iw_sockets_close(sock);
        return out;
    }

    iwrange range{};
    if (iw_get_range_info(sock, ifname, &range) < 0) {
        LOG_WARNING(Network, "iw_get_range_info failed on {}", ifname);
        iw_sockets_close(sock);
        return out;
    }

    wireless_scan_head head{};
    const auto start = std::chrono::steady_clock::now();
    bool have = false;

    while (!have && std::chrono::steady_clock::now() - start < deadline) {
        std::this_thread::sleep_for(100ms);
        if (iw_scan(sock, ifname, range.we_version_compiled, &head) != 0)
            continue;

        out.clear();
        for (auto* ws = head.result; ws; ws = ws->next) {
            if (!ws->b.has_essid)
                continue;

            Network::ScanData sd{};
            sd.ssid_len = static_cast<u8>(std::min<int>(ws->b.essid_len, 0x20));
            std::memcpy(sd.ssid, ws->b.essid, sd.ssid_len);
            sd.quality = QualityToPercent(range, ws);
            sd.flags |= 1;
            if (ws->b.has_key)
                sd.flags |= 2;

            out.emplace_back(sd);
            char tmp[0x22]{};
            std::memcpy(tmp, sd.ssid, sd.ssid_len);
        }
        have = !out.empty();
    }

    iw_sockets_close(sock);
    return out;
}
#endif /* linux */
#endif

std::vector<Network::ScanData> ScanWifiNetworks(std::chrono::milliseconds deadline) {
#ifdef ENABLE_WIFI_SCAN
#if defined(_WIN32)
    return ScanWifiWin(deadline);
#elif defined(__linux__) && !defined(ANDROID)
    return ScanWifiLinux(deadline);
#else
    return {}; // unsupported host, pretend no results
#endif
#else
    return {}; // disabled, pretend no results
#endif
}

} // namespace Network
