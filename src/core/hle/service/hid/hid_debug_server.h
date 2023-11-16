// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::HID {
class ResourceManager;

class IHidDebugServer final : public ServiceFramework<IHidDebugServer> {
public:
    explicit IHidDebugServer(Core::System& system_, std::shared_ptr<ResourceManager> resource);
    ~IHidDebugServer() override;

private:
    std::shared_ptr<ResourceManager> GetResourceManager();

    std::shared_ptr<ResourceManager> resource_manager;
};

} // namespace Service::HID
