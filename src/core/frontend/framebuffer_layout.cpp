// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cmath>

#include "common/assert.h"
#include "common/settings.h"
#include "common/settings_enums.h"
#include "core/frontend/framebuffer_layout.h"

namespace Layout {

// Finds the largest size subrectangle contained in window area that is confined to the aspect ratio
static Common::Rectangle<u32> MaxRectangle(Common::Rectangle<u32> window_area, float screen_aspect_ratio) noexcept {
    const float scale = (std::min)(float(window_area.GetWidth()), float(window_area.GetHeight()) / screen_aspect_ratio);
    return Common::Rectangle<u32>{0, 0, u32(std::round(scale)), u32(std::round(scale * screen_aspect_ratio))};
}

/// @brief Factory method for constructing a default FramebufferLayout
/// @param width Window framebuffer width in pixels
/// @param height Window framebuffer height in pixels
/// @return Newly created FramebufferLayout object with default screen regions initialized
FramebufferLayout DefaultFrameLayout(u32 width, u32 height) noexcept {
    ASSERT(width > 0 && height > 0);
    auto const window_aspect_ratio = float(height) / float(width);
    auto const emulation_aspect_ratio = EmulationAspectRatio(Settings::values.aspect_ratio.GetValue(), window_aspect_ratio);
    Common::Rectangle<u32> const screen_window_area{0, 0, width, height};
    auto screen = MaxRectangle(screen_window_area, emulation_aspect_ratio);
    if (window_aspect_ratio < emulation_aspect_ratio) {
        screen = screen.TranslateX((screen_window_area.GetWidth() - screen.GetWidth()) / 2);
    } else {
        screen = screen.TranslateY((height - screen.GetHeight()) / 2);
    }
    // The drawing code needs at least somewhat valid values for both screens
    // so just calculate them both even if the other isn't showing.
    return FramebufferLayout{
        .width = width,
        .height = height,
        .screen = screen,
        .is_srgb = false,
    };
}

/// @brief Convenience method to determine emulation aspect ratio
/// @param aspect Represents the index of aspect ratio stored in Settings::values.aspect_ratio
/// @param window_aspect_ratio Current window aspect ratio
/// @return Emulation render window aspect ratio
float EmulationAspectRatio(Settings::AspectRatio aspect, float window_aspect_ratio) noexcept {
    switch (aspect) {
    case Settings::AspectRatio::R16_9:
        return 9.0f / 16.0f;
    case Settings::AspectRatio::R4_3:
        return 3.0f / 4.0f;
    case Settings::AspectRatio::R21_9:
        return 9.0f / 21.0f;
    case Settings::AspectRatio::R16_10:
        return 10.0f / 16.0f;
    case Settings::AspectRatio::Stretch:
        return window_aspect_ratio;
    default:
        return float(ScreenUndocked::Height) / ScreenUndocked::Width;
    }
}

} // namespace Layout
