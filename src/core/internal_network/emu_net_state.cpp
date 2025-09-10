// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/logging/log.h"
#include "core/internal_network/emu_net_state.h"
#include "core/internal_network/network.h"
#include "core/internal_network/network_interface.h"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <wlanapi.h>
#ifdef _MSC_VER
#pragma comment(lib, "wlanapi.lib")
#endif
#endif
#include <common/settings.h>

#include <mutex>

namespace Network {

EmuNetState& EmuNetState::Get() {
    static EmuNetState instance;
    return instance;
}

u8 QualityToBars(u8 q) {
    if (q == 0)
        return 0;
    else if (q < 34)
        return 1;
    else if (q < 67)
        return 2;
    else
        return 3;
}

void RefreshFromHost() {
    auto& st = Network::EmuNetState::Get();
    std::scoped_lock lk{st.mtx};

    const auto sel = GetSelectedNetworkInterface();

    if (!sel.has_value()) {
        st.connected = false;
        st.via_wifi = false;
        st.wifi_enabled = false;
        st.ethernet_enabled = false;
        std::memset(st.ssid, 0, sizeof(st.ssid));
        st.secure = false;
        st.bars = 0;
        return;
    }

    st.wifi_enabled = !Settings::values.airplane_mode.GetValue();
    st.ethernet_enabled = sel->kind == HostAdapterKind::Ethernet;

    st.connected = true;
    st.via_wifi = sel->kind == HostAdapterKind::Wifi;
    std::strncpy(st.ssid, sel->name.c_str(), sizeof(st.ssid) - 1);
    st.secure = true;
    st.ip = TranslateIPv4(sel->ip_address);
    st.mask = TranslateIPv4(sel->subnet_mask);
    st.gw = TranslateIPv4(sel->gateway);

#ifdef _WIN32

    if (st.via_wifi) {
        HANDLE hClient{};
        DWORD ver{};
        if (WlanOpenHandle(2, nullptr, &ver, &hClient) == ERROR_SUCCESS) {
            PWLAN_INTERFACE_INFO_LIST ifs{};
            if (WlanEnumInterfaces(hClient, nullptr, &ifs) == ERROR_SUCCESS) {
                const auto& g = ifs->InterfaceInfo[0].InterfaceGuid;
                PWLAN_CONNECTION_ATTRIBUTES attr{};
                DWORD attrSize = 0;
                WLAN_OPCODE_VALUE_TYPE opType{};
                if (WlanQueryInterface(hClient, &g, wlan_intf_opcode_current_connection, nullptr,
                                       &attrSize, reinterpret_cast<PVOID*>(&attr),
                                       &opType) == ERROR_SUCCESS) {

                    const DOT11_SSID& ssid = attr->wlanAssociationAttributes.dot11Ssid;
                    const size_t len = std::min<size_t>(ssid.uSSIDLength, sizeof(st.ssid) - 1);
                    std::memset(st.ssid, 0, sizeof(st.ssid));
                    std::memcpy(st.ssid, ssid.ucSSID, len);

                    st.bars = QualityToBars(
                        static_cast<u8>(attr->wlanAssociationAttributes.wlanSignalQuality));
                    WlanFreeMemory(attr);
                }
                WlanFreeMemory(ifs);
            }
            WlanCloseHandle(hClient, nullptr);
        }
    } else {
        st.bars = 3;
    }
#else
    /* non-Windows stub */
    st.bars = st.via_wifi ? 2 : 3;
#endif
}

} // namespace Network
