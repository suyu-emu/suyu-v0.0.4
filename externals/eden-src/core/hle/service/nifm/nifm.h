// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator
// Project// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::NIFM {

class IScanRequest;
class IRequest;

void LoopProcess(Core::System& system);

class IGeneralService final : public ServiceFramework<IGeneralService> {
public:
    explicit IGeneralService(Core::System& system_);
    ~IGeneralService() override;

private:
    void GetClientId(HLERequestContext& ctx);
    void CreateScanRequest(HLERequestContext& ctx);
    void CreateRequest(HLERequestContext& ctx);
    void GetCurrentNetworkProfile(HLERequestContext& ctx);
    void EnumerateNetworkInterfaces(HLERequestContext& ctx);
    void EnumerateNetworkProfiles(HLERequestContext& ctx);
    void GetNetworkProfile(HLERequestContext& ctx);
    void SetNetworkProfile(HLERequestContext& ctx);
    void RemoveNetworkProfile(HLERequestContext& ctx);
    void GetScanData(HLERequestContext& ctx);
    void GetCurrentIpAddress(HLERequestContext& ctx);
    void CreateTemporaryNetworkProfile(HLERequestContext& ctx);
    void GetCurrentIpConfigInfo(HLERequestContext& ctx);
    void SetWirelessCommunicationEnabled(HLERequestContext& ctx);
    void IsWirelessCommunicationEnabled(HLERequestContext& ctx);
    void GetInternetConnectionStatus(HLERequestContext& ctx);
    void SetEthernetCommunicationEnabled(HLERequestContext& ctx);
    void IsEthernetCommunicationEnabled(HLERequestContext& ctx);
    void IsAnyInternetRequestAccepted(HLERequestContext& ctx);
    void IsAnyForegroundRequestAccepted(HLERequestContext& ctx);
    void GetSsidListVersion(HLERequestContext& ctx);
    void GetScanDataV2(HLERequestContext& ctx);
    void ConfirmSystemAvailability(HLERequestContext& ctx);
    void SetBackgroundRequestEnabled(HLERequestContext& ctx);
    void GetCurrentAccessPoint(HLERequestContext& ctx);
    void GetScanDataV3(HLERequestContext& ctx);
};

} // namespace Service::NIFM
