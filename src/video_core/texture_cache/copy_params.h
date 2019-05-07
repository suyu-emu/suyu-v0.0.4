// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace VideoCommon {

struct CopyParams {
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
