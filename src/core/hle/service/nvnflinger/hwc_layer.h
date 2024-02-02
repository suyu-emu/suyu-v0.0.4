// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/math_util.h"
#include "core/hle/service/nvdrv/nvdata.h"
#include "core/hle/service/nvnflinger/buffer_transform_flags.h"
#include "core/hle/service/nvnflinger/pixel_format.h"
#include "core/hle/service/nvnflinger/ui/fence.h"

namespace Service::Nvnflinger {

struct HwcLayer {
    u32 buffer_handle;
    u32 offset;
    android::PixelFormat format;
    u32 width;
    u32 height;
    u32 stride;
    s32 z_index;
    android::BufferTransformFlags transform;
    Common::Rectangle<int> crop_rect;
    android::Fence acquire_fence;
};

} // namespace Service::Nvnflinger
