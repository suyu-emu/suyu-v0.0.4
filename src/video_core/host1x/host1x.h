// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "video_core/host1x/syncpoint_manager.h"

namespace Tegra {

namespace Host1x {

class Host1x {
public:
    Host1x() : syncpoint_manager{} {}

    SyncpointManager& GetSyncpointManager() {
        return syncpoint_manager;
    }

    const SyncpointManager& GetSyncpointManager() const {
        return syncpoint_manager;
    }

private:
    SyncpointManager syncpoint_manager;
};

} // namespace Host1x

} // namespace Tegra
