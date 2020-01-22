// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "yuzu/uisettings.h"

namespace UISettings {

const Themes themes{{
    {"Light", "default"},
    {"Light Colorful", "colorful"},
    {"Dark", "qdarkstyle"},
    {"Dark Colorful", "colorful_dark"},
}};

Values values = {};
} // namespace UISettings
