// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/math_util.h"
#include "core/hle/service/nvflinger/pixel_format.h"

namespace Tegra {

/**
 * Struct describing framebuffer configuration
 */
struct FramebufferConfig {
    enum class TransformFlags : u32 {
        /// No transform flags are set
        Unset = 0x00,
        /// Flip source image horizontally (around the vertical axis)
        FlipH = 0x01,
        /// Flip source image vertically (around the horizontal axis)
        FlipV = 0x02,
        /// Rotate source image 90 degrees clockwise
        Rotate90 = 0x04,
        /// Rotate source image 180 degrees
        Rotate180 = 0x03,
        /// Rotate source image 270 degrees clockwise
        Rotate270 = 0x07,
    };

    VAddr address{};
    u32 offset{};
    u32 width{};
    u32 height{};
    u32 stride{};

    TransformFlags transform_flags{};
    android::PixelFormat pixel_format{};
    Common::Rectangle<int> crop_rect;
};

} // namespace Tegra
