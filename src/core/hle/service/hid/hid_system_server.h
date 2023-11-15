// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Kernel {
class KEvent;
}

namespace Service::HID {
class ResourceManager;

class IHidSystemServer final : public ServiceFramework<IHidSystemServer> {
public:
    explicit IHidSystemServer(Core::System& system_, std::shared_ptr<ResourceManager> resource);
    ~IHidSystemServer() override;

private:
    void ApplyNpadSystemCommonPolicy(HLERequestContext& ctx);
    void GetLastActiveNpad(HLERequestContext& ctx);
    void GetUniquePadsFromNpad(HLERequestContext& ctx);
    void AcquireJoyDetachOnBluetoothOffEventHandle(HLERequestContext& ctx);
    void IsUsbFullKeyControllerEnabled(HLERequestContext& ctx);
    void GetTouchScreenDefaultConfiguration(HLERequestContext& ctx);

    std::shared_ptr<ResourceManager> GetResourceManager();

    Kernel::KEvent* joy_detach_event;
    KernelHelpers::ServiceContext service_context;
    std::shared_ptr<ResourceManager> resource_manager;
};

} // namespace Service::HID
