// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"

#include "video_core/host1x/gpu_device_memory_manager.h"
#include "video_core/host1x/syncpoint_manager.h"

namespace Core {
class System;
} // namespace Core

namespace Tegra {

namespace Host1x {

class Host1x {
public:
    explicit Host1x(Core::System& system);

    SyncpointManager& GetSyncpointManager() {
        return syncpoint_manager;
    }

    const SyncpointManager& GetSyncpointManager() const {
        return syncpoint_manager;
    }

    Tegra::MaxwellDeviceMemoryManager& MemoryManager() {
        return memory_manager;
    }

    const Tegra::MaxwellDeviceMemoryManager& MemoryManager() const {
        return memory_manager;
    }

private:
    Core::System& system;
    SyncpointManager syncpoint_manager;
    Tegra::MaxwellDeviceMemoryManager memory_manager;
};

} // namespace Host1x

} // namespace Tegra
