// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <ankerl/unordered_dense.h>

#include "qt_common/config/qt_config.h"

namespace Core {
class System;
}

class Config;

class InputProfiles {

public:
    explicit InputProfiles();
    virtual ~InputProfiles();

    std::vector<std::string> GetInputProfileNames();

    static bool IsProfileNameValid(std::string_view profile_name);

    bool CreateProfile(const std::string& profile_name, std::size_t player_index);
    bool DeleteProfile(const std::string& profile_name);
    bool LoadProfile(const std::string& profile_name, std::size_t player_index);
    bool SaveProfile(const std::string& profile_name, std::size_t player_index);

private:
    bool ProfileExistsInMap(const std::string& profile_name) const;

    ankerl::unordered_dense::map<std::string, std::unique_ptr<QtConfig>> map_profiles;
};
