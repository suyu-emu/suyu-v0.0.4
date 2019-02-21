// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"

namespace Service::NVFlinger {
class BufferQueue;
}

namespace Service::VI {

struct Layer {
    Layer(u64 id, std::shared_ptr<NVFlinger::BufferQueue> queue);
    ~Layer();

    u64 id;
    std::shared_ptr<NVFlinger::BufferQueue> buffer_queue;
};

} // namespace Service::VI
