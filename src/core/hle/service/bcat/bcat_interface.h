// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::FileSystem {
class FileSystemController;
}

namespace Service::BCAT {
class BcatBackend;
class IBcatService;
class IDeliveryCacheStorageService;

class BcatInterface final : public ServiceFramework<BcatInterface> {
public:
    explicit BcatInterface(Core::System& system_, const char* name_);
    ~BcatInterface() override;

private:
    Result CreateBcatService(OutInterface<IBcatService> out_interface);

    Result CreateDeliveryCacheStorageService(
        OutInterface<IDeliveryCacheStorageService> out_interface);

    Result CreateDeliveryCacheStorageServiceWithApplicationId(
        u64 title_id, OutInterface<IDeliveryCacheStorageService> out_interface);

    std::unique_ptr<BcatBackend> backend;
    Service::FileSystem::FileSystemController& fsc;
};

} // namespace Service::BCAT
