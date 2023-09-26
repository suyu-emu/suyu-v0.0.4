// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <common/settings_common.h>
#include "common/common_types.h"
#include "common/settings_setting.h"

namespace AndroidSettings {

struct Values {
    Settings::Linkage linkage;

    // Android
    Settings::Setting<bool> picture_in_picture{linkage, true, "picture_in_picture",
                                               Settings::Category::Android};
    Settings::Setting<s32> screen_layout{linkage,
                                         5,
                                         "screen_layout",
                                         Settings::Category::Android,
                                         Settings::Specialization::Default,
                                         true,
                                         true};
};

extern Values values;

} // namespace AndroidSettings
