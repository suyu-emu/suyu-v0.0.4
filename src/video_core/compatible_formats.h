// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <bitset>
#include <cstddef>

#include "video_core/surface.h"

namespace VideoCore::Surface {

class FormatCompatibility {
public:
    using Table = std::array<std::bitset<MaxPixelFormat>, MaxPixelFormat>;

    explicit FormatCompatibility();

    bool TestView(PixelFormat format_a, PixelFormat format_b) const noexcept {
        return view[static_cast<size_t>(format_a)][static_cast<size_t>(format_b)];
    }

    bool TestCopy(PixelFormat format_a, PixelFormat format_b) const noexcept {
        return copy[static_cast<size_t>(format_a)][static_cast<size_t>(format_b)];
    }

private:
    Table view;
    Table copy;
};

} // namespace VideoCore::Surface
