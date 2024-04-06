// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "video_core/renderer_metal/objc_bridge.h"

namespace Metal {

class Device {
public:
    explicit Device();
    ~Device();

    MTLDevice_t GetDevice() const {
        return device;
    }

    MTLCommandQueue_t GetCommandQueue() const {
        return command_queue;
    }

private:
    MTLDevice_t device;
    MTLCommandQueue_t command_queue;
};

} // namespace Metal
