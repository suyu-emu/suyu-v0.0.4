// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/guest_driver.h"

namespace VideoCore {

void GuestDriverProfile::DeduceTextureHandlerSize(std::vector<u32>&& bound_offsets) {
    if (texture_handler_size_deduced) {
        return;
    }
    std::size_t size = bound_offsets.size();
    if (size < 2) {
        return;
    }
    std::sort(bound_offsets.begin(), bound_offsets.end(),
              [](const u32& a, const u32& b) { return a < b; });
    u32 min_val = 0xFFFFFFFF; // set to highest possible 32 bit integer;
    for (std::size_t i = 1; i < size; i++) {
        if (bound_offsets[i] == bound_offsets[i - 1]) {
            continue;
        }
        const u32 new_min = bound_offsets[i] - bound_offsets[i - 1];
        min_val = std::min(min_val, new_min);
    }
    if (min_val > 2) {
        return;
    }
    texture_handler_size_deduced = true;
    texture_handler_size = sizeof(u32) * min_val;
}

} // namespace VideoCore
