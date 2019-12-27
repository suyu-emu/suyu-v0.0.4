// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace VideoCommon::Dirty {

enum : u8 {
    NullEntry = 0,

    RenderTargets,
    ColorBuffer0,
    ColorBuffer1,
    ColorBuffer2,
    ColorBuffer3,
    ColorBuffer4,
    ColorBuffer5,
    ColorBuffer6,
    ColorBuffer7,
    ZetaBuffer,

    LastCommonEntry,
};

} // namespace VideoCommon::Dirty
