// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"

namespace Layout {

enum ScreenUndocked : u32 {
    Width = 1280,
    Height = 720,
};

enum ScreenDocked : u32 {
    WidthDocked = 1920,
    HeightDocked = 1080,
};

/// Describes the layout of the window framebuffer
struct FramebufferLayout {
    u32 width{ScreenUndocked::Width};
    u32 height{ScreenUndocked::Height};

    Common::Rectangle<u32> screen;

    /**
     * Returns the ration of pixel size of the screen, compared to the native size of the undocked
     * Switch screen.
     */
    float GetScalingRatio() const {
        return static_cast<float>(screen.GetWidth()) / ScreenUndocked::Width;
    }
};

/**
 * Factory method for constructing a default FramebufferLayout
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout DefaultFrameLayout(u32 width, u32 height);

/**
 * Convenience method to get frame layout by resolution scale
 * @param res_scale resolution scale factor
 */
FramebufferLayout FrameLayoutFromResolutionScale(u32 res_scale);

} // namespace Layout
