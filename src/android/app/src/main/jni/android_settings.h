// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <common/settings_common.h>
#include "common/common_types.h"
#include "common/settings_setting.h"

namespace AndroidSettings {

struct GameDir {
    std::string path;
    bool deep_scan = false;
};

struct Values {
    Settings::Linkage linkage;

    // Path settings
    std::vector<GameDir> game_dirs;

    // Android
    Settings::Setting<bool> picture_in_picture{linkage, false, "picture_in_picture",
                                               Settings::Category::Android};
    Settings::Setting<s32> screen_layout{linkage,
                                         5,
                                         "screen_layout",
                                         Settings::Category::Android,
                                         Settings::Specialization::Default,
                                         true,
                                         true};

    Settings::SwitchableSetting<std::string, false> driver_path{linkage, "", "driver_path",
                                                                Settings::Category::GpuDriver};
};

extern Values values;

} // namespace AndroidSettings
