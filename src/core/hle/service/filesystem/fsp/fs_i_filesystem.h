// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/file_sys/vfs.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/filesystem/fsp/fsp_util.h"
#include "core/hle/service/service.h"

namespace Service::FileSystem {

class IFileSystem final : public ServiceFramework<IFileSystem> {
public:
    explicit IFileSystem(Core::System& system_, FileSys::VirtualDir backend_, SizeGetter size_);

    void CreateFile(HLERequestContext& ctx);
    void DeleteFile(HLERequestContext& ctx);
    void CreateDirectory(HLERequestContext& ctx);
    void DeleteDirectory(HLERequestContext& ctx);
    void DeleteDirectoryRecursively(HLERequestContext& ctx);
    void CleanDirectoryRecursively(HLERequestContext& ctx);
    void RenameFile(HLERequestContext& ctx);
    void OpenFile(HLERequestContext& ctx);
    void OpenDirectory(HLERequestContext& ctx);
    void GetEntryType(HLERequestContext& ctx);
    void Commit(HLERequestContext& ctx);
    void GetFreeSpaceSize(HLERequestContext& ctx);
    void GetTotalSpaceSize(HLERequestContext& ctx);
    void GetFileTimeStampRaw(HLERequestContext& ctx);
    void GetFileSystemAttribute(HLERequestContext& ctx);

private:
    VfsDirectoryServiceWrapper backend;
    SizeGetter size;
};

} // namespace Service::FileSystem
