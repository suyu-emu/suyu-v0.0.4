// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

namespace OpenGL {

class Device {
public:
    Device();

    void Initialize();

    std::size_t GetUniformBufferAlignment() const {
        return uniform_buffer_alignment;
    }

private:
    std::size_t uniform_buffer_alignment{};
};

} // namespace OpenGL
