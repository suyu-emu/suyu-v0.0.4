// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

namespace VideoCore {

/**
 * The GuestDriverProfile class is used to learn about the GPU drivers behavior and collect
 * information necessary for impossible to avoid HLE methods like shader tracks as they are
 * Entscheidungsproblems.
 */
class GuestDriverProfile {
public:
    void DeduceTextureHandlerSize(std::vector<u32>&& bound_offsets);

    u32 GetTextureHandlerSize() const {
        return texture_handler_size;
    }

    bool TextureHandlerSizeKnown() const {
        return texture_handler_size_deduced;
    }

private:
    // Minimum size of texture handler any driver can use.
    static constexpr u32 min_texture_handler_size = 4;
    // This goes with Vulkan and OpenGL standards but Nvidia GPUs can easily
    // use 4 bytes instead. Thus, certain drivers may squish the size.
    static constexpr u32 default_texture_handler_size = 8;

    u32 texture_handler_size = default_texture_handler_size;
    bool texture_handler_size_deduced = false;
};

} // namespace VideoCore
