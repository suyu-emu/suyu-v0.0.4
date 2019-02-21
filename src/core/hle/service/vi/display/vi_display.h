// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>

#include "common/common_types.h"
#include "core/hle/kernel/writable_event.h"

namespace Service::VI {

struct Layer;

struct Display {
    Display(u64 id, std::string name);
    ~Display();

    u64 id;
    std::string name;

    std::vector<Layer> layers;
    Kernel::EventPair vsync_event;
};

} // namespace Service::VI
