// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "core/hle/service/service.h"

namespace FileSys {
class FileSystemBackend;
}

namespace Service::FileSystem {

class FSP_SRV final : public ServiceFramework<FSP_SRV> {
public:
    explicit FSP_SRV();
    ~FSP_SRV() override;

private:
    void SetCurrentProcess(Kernel::HLERequestContext& ctx);
    void OpenFileSystemWithPatch(Kernel::HLERequestContext& ctx);
    void OpenSdCardFileSystem(Kernel::HLERequestContext& ctx);
    void CreateSaveDataFileSystem(Kernel::HLERequestContext& ctx);
    void OpenSaveDataFileSystem(Kernel::HLERequestContext& ctx);
    void OpenReadOnlySaveDataFileSystem(Kernel::HLERequestContext& ctx);
    void OpenSaveDataInfoReaderBySaveDataSpaceId(Kernel::HLERequestContext& ctx);
    void GetGlobalAccessLogMode(Kernel::HLERequestContext& ctx);
    void OpenDataStorageByCurrentProcess(Kernel::HLERequestContext& ctx);
    void OpenDataStorageByDataId(Kernel::HLERequestContext& ctx);
    void OpenPatchDataStorageByCurrentProcess(Kernel::HLERequestContext& ctx);

    FileSys::VirtualFile romfs;
    u64 current_process_id = 0;
};

} // namespace Service::FileSystem
