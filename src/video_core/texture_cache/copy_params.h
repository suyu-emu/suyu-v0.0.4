// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace VideoCommon {

struct CopyParams {
    constexpr CopyParams(u32 source_x_, u32 source_y_, u32 source_z_, u32 dest_x_, u32 dest_y_,
                         u32 dest_z_, u32 source_level_, u32 dest_level_, u32 width_, u32 height_,
                         u32 depth_)
        : source_x{source_x_}, source_y{source_y_}, source_z{source_z_}, dest_x{dest_x_},
          dest_y{dest_y_}, dest_z{dest_z_}, source_level{source_level_},
          dest_level{dest_level_}, width{width_}, height{height_}, depth{depth_} {}

    constexpr CopyParams(u32 width_, u32 height_, u32 depth_, u32 level_)
        : source_x{}, source_y{}, source_z{}, dest_x{}, dest_y{}, dest_z{}, source_level{level_},
          dest_level{level_}, width{width_}, height{height_}, depth{depth_} {}

    u32 source_x;
    u32 source_y;
    u32 source_z;
    u32 dest_x;
    u32 dest_y;
    u32 dest_z;
    u32 source_level;
    u32 dest_level;
    u32 width;
    u32 height;
    u32 depth;
};

} // namespace VideoCommon
