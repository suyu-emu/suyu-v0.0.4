// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <vector>

#include "common/common_types.h"

namespace Core {

class System;

class NintendoSwitchLibrary {
public:
    struct GameInfo {
        u64 program_id;
        std::string title;
        std::string file_path;
    };

    explicit NintendoSwitchLibrary(Core::System& system);

    std::vector<GameInfo> GetInstalledGames();
    std::string GetGameName(u64 program_id);
    bool LaunchGame(u64 program_id);

private:
    Core::System& system;
};

} // namespace Core