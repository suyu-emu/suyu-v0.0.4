// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <random>
#include <frozen/string.h>
#include "common/settings.h"
#include "settings_generator.h"

namespace FrontendCommon {

void GenerateSettings() {
    static std::random_device rd;

    // Web Token
    if (Settings::values.eden_token.GetValue().empty()) {
        static constexpr const size_t token_length = 48;
        static constexpr const frozen::string token_set = "abcdefghijklmnopqrstuvwxyz";
        static std::uniform_int_distribution<int> token_dist(0, token_set.size() - 1);
        std::string result;

        for (size_t i = 0; i < token_length; ++i) {
            size_t idx = token_dist(rd);
            result += token_set[idx];
        }
        Settings::values.eden_token.SetValue(result);
    }

    // Randomly generated number because, well, we fill the rest automagically ;)
    // Other serial parts are filled by Region_Index
    std::random_device device;
    std::mt19937 gen(device());
    std::uniform_int_distribution<u32> distribution(1, (std::numeric_limits<u32>::max)());
    if (Settings::values.serial_unit.GetValue() == 0)
        Settings::values.serial_unit.SetValue(distribution(gen));
    if (Settings::values.serial_battery.GetValue() == 0)
        Settings::values.serial_battery.SetValue(distribution(gen));
}

}
