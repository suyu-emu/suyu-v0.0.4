// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "common/math_util.h"

namespace Settings {
enum class AspectRatio : u32;
}

namespace Layout {

namespace MinimumSize {
constexpr u32 Width = 640;
constexpr u32 Height = 360;
} // namespace MinimumSize

namespace ScreenUndocked {
constexpr u32 Width = 1280;
constexpr u32 Height = 720;
} // namespace ScreenUndocked

namespace ScreenDocked {
constexpr u32 Width = 1920;
constexpr u32 Height = 1080;
} // namespace ScreenDocked

/// @brief Describes the layout of the window framebuffer
struct FramebufferLayout {
    u32 width{ScreenUndocked::Width};
    u32 height{ScreenUndocked::Height};
    Common::Rectangle<u32> screen;
    bool is_srgb{};
};

FramebufferLayout DefaultFrameLayout(u32 width, u32 height) noexcept;
float EmulationAspectRatio(Settings::AspectRatio aspect, float window_aspect_ratio) noexcept;

} // namespace Layout
