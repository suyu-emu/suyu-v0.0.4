// Copyright 2021 yuzu emulator team
// Copyright 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/nvdrv/core/container.h"
#include "core/hle/service/nvdrv/core/nvmap.h"
#include "core/hle/service/nvdrv/core/syncpoint_manager.h"
#include "video_core/gpu.h"

namespace Service::Nvidia::NvCore {

struct ContainerImpl {
    ContainerImpl(Tegra::GPU& gpu_) : file{}, manager{gpu_} {}
    NvMap file;
    SyncpointManager manager;
};

Container::Container(Tegra::GPU& gpu_) {
    impl = std::make_unique<ContainerImpl>(gpu_);
}

Container::~Container() = default;

NvMap& Container::GetNvMapFile() {
    return impl->file;
}

const NvMap& Container::GetNvMapFile() const {
    return impl->file;
}

SyncpointManager& Container::GetSyncpointManager() {
    return impl->manager;
}

const SyncpointManager& Container::GetSyncpointManager() const {
    return impl->manager;
}

} // namespace Service::Nvidia::NvCore
