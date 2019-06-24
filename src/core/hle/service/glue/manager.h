// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/file_sys/control_metadata.h"
#include "core/file_sys/romfs_factory.h"
#include "core/hle/result.h"

namespace Service::Glue {

struct ApplicationLaunchProperty {
    u64 title_id;
    u32 version;
    FileSys::StorageId base_game_storage_id;
    FileSys::StorageId update_storage_id;
    INSERT_PADDING_BYTES(0x2);
};
static_assert(sizeof(ApplicationLaunchProperty) == 0x10,
              "ApplicationLaunchProperty has incorrect size.");

class ARPManager {
public:
    ARPManager();
    ~ARPManager();

    ResultVal<ApplicationLaunchProperty> GetLaunchProperty(u64 title_id) const;
    ResultVal<std::vector<u8>> GetControlProperty(u64 title_id) const;

    ResultCode Register(u64 title_id, ApplicationLaunchProperty launch, std::vector<u8> control);

    ResultCode Unregister(u64 title_id);

    void ResetAll();

private:
    struct MapEntry {
        ApplicationLaunchProperty launch;
        std::vector<u8> control;
    };

    std::map<u64, MapEntry> entries;
};

} // namespace Service::Glue
