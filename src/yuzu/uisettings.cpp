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
        // TODO: Move this function wherever appropriate to implement Stretched aspect
        break;
    }
    return height * 16 / 9;
}

} // namespace UISettings
