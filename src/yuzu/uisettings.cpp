// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "yuzu/uisettings.h"

#ifndef CANNOT_EXPLICITLY_INSTANTIATE
namespace Settings {
template class Setting<bool>;
template class Setting<std::string>;
template class Setting<u16, true>;
template class Setting<u32>;
template class Setting<u8, true>;
template class Setting<u8>;
template class Setting<unsigned long long>;
} // namespace Settings
#endif

namespace UISettings {

const Themes themes{{
    {"Default", "default"},
    {"Default Colorful", "colorful"},
    {"Dark", "qdarkstyle"},
    {"Dark Colorful", "colorful_dark"},
    {"Midnight Blue", "qdarkstyle_midnight_blue"},
    {"Midnight Blue Colorful", "colorful_midnight_blue"},
}};

bool IsDarkTheme() {
    const auto& theme = UISettings::values.theme;
    return theme == QStringLiteral("qdarkstyle") ||
           theme == QStringLiteral("qdarkstyle_midnight_blue") ||
           theme == QStringLiteral("colorful_dark") ||
           theme == QStringLiteral("colorful_midnight_blue");
}

Values values = {};

u32 CalculateWidth(u32 height, Settings::AspectRatio ratio) {
    switch (ratio) {
    case Settings::AspectRatio::R4_3:
        return height * 4 / 3;
    case Settings::AspectRatio::R21_9:
        return height * 21 / 9;
    case Settings::AspectRatio::R16_10:
        return height * 16 / 10;
    case Settings::AspectRatio::R16_9:
    case Settings::AspectRatio::Stretch:
        break;
    }
    return height * 16 / 9;
}

Settings::AspectRatio ConvertScreenshotRatioToRatio(Settings::ScreenshotAspectRatio ratio) {
    switch (ratio) {
    case Settings::ScreenshotAspectRatio::Auto:
        return Settings::values.aspect_ratio.GetValue();
    case Settings::ScreenshotAspectRatio::R16_9:
        return Settings::AspectRatio::R16_9;
    case Settings::ScreenshotAspectRatio::R4_3:
        return Settings::AspectRatio::R4_3;
    case Settings::ScreenshotAspectRatio::R21_9:
        return Settings::AspectRatio::R21_9;
    case Settings::ScreenshotAspectRatio::R16_10:
        return Settings::AspectRatio::R16_10;
    case Settings::ScreenshotAspectRatio::Unspecified:
        break;
    }
    return Settings::AspectRatio::R16_9;
}

} // namespace UISettings
