// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/bcat/bcat_result.h"
#include "core/hle/service/bcat/delivery_cache_directory_service.h"
#include "core/hle/service/bcat/delivery_cache_file_service.h"
#include "core/hle/service/bcat/delivery_cache_storage_service.h"
#include "core/hle/service/cmif_serialization.h"

namespace Service::BCAT {

IDeliveryCacheStorageService::IDeliveryCacheStorageService(Core::System& system_,
                                                           FileSys::VirtualDir root_)
    : ServiceFramework{system_, "IDeliveryCacheStorageService"}, root(std::move(root_)) {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, C<&IDeliveryCacheStorageService::CreateFileService>, "CreateFileService"},
        {1, C<&IDeliveryCacheStorageService::CreateDirectoryService>, "CreateDirectoryService"},
        {2, C<&IDeliveryCacheStorageService::EnumerateDeliveryCacheDirectory>, "EnumerateDeliveryCacheDirectory"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IDeliveryCacheStorageService::~IDeliveryCacheStorageService() = default;

Result IDeliveryCacheStorageService::CreateFileService(
    OutInterface<IDeliveryCacheFileService> out_interface) {
    LOG_DEBUG(Service_BCAT, "called");

    *out_interface = std::make_shared<IDeliveryCacheFileService>(system, root);
    R_SUCCEED();
}

Result IDeliveryCacheStorageService::CreateDirectoryService(
    OutInterface<IDeliveryCacheDirectoryService> out_interface) {
    LOG_DEBUG(Service_BCAT, "called");

    *out_interface = std::make_shared<IDeliveryCacheDirectoryService>(system, root);
    R_SUCCEED();
}

Result IDeliveryCacheStorageService::EnumerateDeliveryCacheDirectory(
    Out<u32> out_directories_size,
    OutArray<DirectoryName, BufferAttr_HipcMapAlias> out_directories) {
    LOG_DEBUG(Service_BCAT, "called, size={:016X}", out_directories.size());

    *out_directories_size =
        static_cast<u32>(std::min(out_directories.size(), entries.size() - next_read_index));
    memcpy(out_directories.data(), entries.data() + next_read_index,
           *out_directories_size * sizeof(DirectoryName));
    next_read_index += *out_directories_size;
    R_SUCCEED();
}

} // namespace Service::BCAT
