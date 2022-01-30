// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "core/hle/service/nvdrv/core/syncpoint_manager.h"
#include "video_core/host1x/host1x.h"

namespace Service::Nvidia::NvCore {

SyncpointManager::SyncpointManager(Tegra::Host1x::Host1x& host1x_) : host1x{host1x_} {}

SyncpointManager::~SyncpointManager() = default;

u32 SyncpointManager::RefreshSyncpoint(u32 syncpoint_id) {
    syncpoints[syncpoint_id].min = host1x.GetSyncpointManager().GetHostSyncpointValue(syncpoint_id);
    return GetSyncpointMin(syncpoint_id);
}

u32 SyncpointManager::AllocateSyncpoint() {
    for (u32 syncpoint_id = 1; syncpoint_id < MaxSyncPoints; syncpoint_id++) {
        if (!syncpoints[syncpoint_id].is_allocated) {
            syncpoints[syncpoint_id].is_allocated = true;
            return syncpoint_id;
        }
    }
    ASSERT_MSG(false, "No more available syncpoints!");
    return {};
}

u32 SyncpointManager::IncreaseSyncpoint(u32 syncpoint_id, u32 value) {
    for (u32 index = 0; index < value; ++index) {
        syncpoints[syncpoint_id].max.fetch_add(1, std::memory_order_relaxed);
    }

    return GetSyncpointMax(syncpoint_id);
}

} // namespace Service::Nvidia::NvCore
