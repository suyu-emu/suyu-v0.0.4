// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <random>
#include <frozen/string.h>
#include "common/settings.h"
#include "settings_generator.h"

namespace FrontendCommon {

void GenerateSettings() {
    static std::random_device rd;

    // Web Token //
    auto &token_setting = Settings::values.eden_token;
    if (token_setting.GetValue().empty()) {
        static constexpr const size_t token_length = 48;
        static constexpr const frozen::string token_set = "abcdefghijklmnopqrstuvwxyz";
        static std::uniform_int_distribution<int> token_dist(0, token_set.size() - 1);
        std::string result;

        for (size_t i = 0; i < token_length; ++i) {
            size_t idx = token_dist(rd);
            result += token_set[idx];
        }

        token_setting.SetValue(result);
    }
}

}
