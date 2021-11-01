// Copyright 2021 yuzu emulator team
// Copyright 2021 Skyline Team and Contributors (https://github.com/skyline-emu/)
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

namespace Tegra {
class GPU;
}

namespace Service::Nvidia::NvCore {

class NvMap;
class SyncpointManager;

struct ContainerImpl;

class Container {
public:
    Container(Tegra::GPU& gpu_);
    ~Container();

    NvMap& GetNvMapFile();

    const NvMap& GetNvMapFile() const;

    SyncpointManager& GetSyncpointManager();

    const SyncpointManager& GetSyncpointManager() const;

private:
    std::unique_ptr<ContainerImpl> impl;
};

} // namespace Service::Nvidia::NvCore
