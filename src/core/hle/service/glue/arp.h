// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service::Glue {

class ARPManager;
class IRegistrar;

class ARP_R final : public ServiceFramework<ARP_R> {
public:
    explicit ARP_R(const Core::System& system, const ARPManager& manager);
    ~ARP_R() override;

private:
    void GetApplicationLaunchProperty(Kernel::HLERequestContext& ctx);
    void GetApplicationLaunchPropertyWithApplicationId(Kernel::HLERequestContext& ctx);
    void GetApplicationControlProperty(Kernel::HLERequestContext& ctx);
    void GetApplicationControlPropertyWithApplicationId(Kernel::HLERequestContext& ctx);

    const Core::System& system;
    const ARPManager& manager;
};

class ARP_W final : public ServiceFramework<ARP_W> {
public:
    explicit ARP_W(const Core::System& system, ARPManager& manager);
    ~ARP_W() override;

private:
    void AcquireRegistrar(Kernel::HLERequestContext& ctx);
    void DeleteProperties(Kernel::HLERequestContext& ctx);

    const Core::System& system;
    ARPManager& manager;
    std::shared_ptr<IRegistrar> registrar;
};

} // namespace Service::Glue
