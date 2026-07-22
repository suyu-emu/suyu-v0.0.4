// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>

namespace Metal {

class Device {
public:
    explicit Device();
    ~Device();

    MTL::Device* GetDevice() const {
        return device;
    }

    MTL::CommandQueue* GetCommandQueue() const {
        return command_queue;
    }

private:
    MTL::Device* device;
    MTL::CommandQueue* command_queue;
};

} // namespace Metal
