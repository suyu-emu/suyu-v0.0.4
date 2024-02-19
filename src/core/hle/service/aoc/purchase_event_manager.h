// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/os/event.h"
#include "core/hle/service/service.h"

namespace Service::AOC {

class IPurchaseEventManager final : public ServiceFramework<IPurchaseEventManager> {
public:
    explicit IPurchaseEventManager(Core::System& system_);
    ~IPurchaseEventManager() override;

    void SetDefaultDeliveryTarget(HLERequestContext& ctx);
    void SetDeliveryTarget(HLERequestContext& ctx);
    void GetPurchasedEventReadableHandle(HLERequestContext& ctx);
    void PopPurchasedProductInfo(HLERequestContext& ctx);
    void PopPurchasedProductInfoWithUid(HLERequestContext& ctx);

private:
    KernelHelpers::ServiceContext service_context;
    Kernel::KEvent* purchased_event;
};

} // namespace Service::AOC
