// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/file_sys/vfs/vfs.h"
#include "core/hle/service/bcat/bcat_types.h"
#include "core/hle/service/cmif_types.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::BCAT {

class IDeliveryCacheDirectoryService final
    : public ServiceFramework<IDeliveryCacheDirectoryService> {
public:
    explicit IDeliveryCacheDirectoryService(Core::System& system_, FileSys::VirtualDir root_);
    ~IDeliveryCacheDirectoryService() override;

private:
    Result Open(DirectoryName dir_name_raw);
    Result Read(Out<u32> out_buffer_size,
                OutArray<DeliveryCacheDirectoryEntry, BufferAttr_HipcMapAlias> out_buffer);
    Result GetCount(Out<u32> out_count);

    FileSys::VirtualDir root;
    FileSys::VirtualDir current_dir;
};

} // namespace Service::BCAT
