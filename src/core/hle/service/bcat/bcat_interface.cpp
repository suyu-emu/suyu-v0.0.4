// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/bcat/bcat_interface.h"
#include "core/hle/service/bcat/bcat_service.h"
#include "core/hle/service/bcat/delivery_cache_storage_service.h"
#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/filesystem/filesystem.h"

namespace Service::BCAT {

std::unique_ptr<BcatBackend> CreateBackendFromSettings([[maybe_unused]] Core::System& system,
                                                       DirectoryGetter getter) {
    return std::make_unique<NullBcatBackend>(std::move(getter));
}

BcatInterface::BcatInterface(Core::System& system_, const char* name_)
    : ServiceFramework{system_, name_}, fsc{system.GetFileSystemController()} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, C<&BcatInterface::CreateBcatService>, "CreateBcatService"},
        {1, C<&BcatInterface::CreateDeliveryCacheStorageService>, "CreateDeliveryCacheStorageService"},
        {2, C<&BcatInterface::CreateDeliveryCacheStorageServiceWithApplicationId>, "CreateDeliveryCacheStorageServiceWithApplicationId"},
        {3, nullptr, "CreateDeliveryCacheProgressService"},
        {4, nullptr, "CreateDeliveryCacheProgressServiceWithApplicationId"},
    };
    // clang-format on

    RegisterHandlers(functions);

    backend =
        CreateBackendFromSettings(system_, [this](u64 tid) { return fsc.GetBCATDirectory(tid); });
}

BcatInterface::~BcatInterface() = default;

Result BcatInterface::CreateBcatService(OutInterface<IBcatService> out_interface) {
    LOG_INFO(Service_BCAT, "called");
    *out_interface = std::make_shared<IBcatService>(system, *backend);
    R_SUCCEED();
}

Result BcatInterface::CreateDeliveryCacheStorageService(
    OutInterface<IDeliveryCacheStorageService> out_interface) {
    LOG_INFO(Service_BCAT, "called");

    const auto title_id = system.GetApplicationProcessProgramID();
    *out_interface =
        std::make_shared<IDeliveryCacheStorageService>(system, fsc.GetBCATDirectory(title_id));
    R_SUCCEED();
}

Result BcatInterface::CreateDeliveryCacheStorageServiceWithApplicationId(
    u64 title_id, OutInterface<IDeliveryCacheStorageService> out_interface) {
    LOG_DEBUG(Service_BCAT, "called, title_id={:016X}", title_id);
    *out_interface =
        std::make_shared<IDeliveryCacheStorageService>(system, fsc.GetBCATDirectory(title_id));
    R_SUCCEED();
}

} // namespace Service::BCAT
