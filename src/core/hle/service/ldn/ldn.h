// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/result.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/ldn/ldn_results.h"
#include "core/hle/service/ldn/ldn_types.h"
#include "core/hle/service/sm/sm.h"

namespace Core {
class System;
}

namespace Service::SM {
class ServiceManager;
}

namespace Service::LDN {

/// Registers all LDN services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& sm, Core::System& system);

class IUserLocalCommunicationService final
    : public ServiceFramework<IUserLocalCommunicationService> {
public:
    explicit IUserLocalCommunicationService(Core::System& system_);
    ~IUserLocalCommunicationService() override;

    void GetState(Kernel::HLERequestContext& ctx);

    void GetNetworkInfo(Kernel::HLERequestContext& ctx);

    void GetDisconnectReason(Kernel::HLERequestContext& ctx);

    void GetSecurityParameter(Kernel::HLERequestContext& ctx);

    void GetNetworkConfig(Kernel::HLERequestContext& ctx);

    void AttachStateChangeEvent(Kernel::HLERequestContext& ctx);

    void GetNetworkInfoLatestUpdate(Kernel::HLERequestContext& ctx);

    void Scan(Kernel::HLERequestContext& ctx);
    void ScanPrivate(Kernel::HLERequestContext& ctx);
    void ScanImpl(Kernel::HLERequestContext& ctx, bool is_private = false);

    void OpenAccessPoint(Kernel::HLERequestContext& ctx);

    void CloseAccessPoint(Kernel::HLERequestContext& ctx);

    void CreateNetwork(Kernel::HLERequestContext& ctx);
    void CreateNetworkPrivate(Kernel::HLERequestContext& ctx);
    void CreateNetworkImpl(Kernel::HLERequestContext& ctx, bool is_private);

    void DestroyNetwork(Kernel::HLERequestContext& ctx);

    void SetAdvertiseData(Kernel::HLERequestContext& ctx);

    void SetStationAcceptPolicy(Kernel::HLERequestContext& ctx);

    void AddAcceptFilterEntry(Kernel::HLERequestContext& ctx);

    void OpenStation(Kernel::HLERequestContext& ctx);

    void CloseStation(Kernel::HLERequestContext& ctx);

    void Disconnect(Kernel::HLERequestContext& ctx);

    void Connect(Kernel::HLERequestContext& ctx);

    void Initialize(Kernel::HLERequestContext& ctx);

    void Finalize(Kernel::HLERequestContext& ctx);

    void Initialize2(Kernel::HLERequestContext& ctx);
    Result InitializeImpl(Kernel::HLERequestContext& ctx);

private:
    void OnEventFired();

    KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* state_change_event;
    Network::RoomNetwork& room_network;

    bool is_initialized{};
};

} // namespace Service::LDN
