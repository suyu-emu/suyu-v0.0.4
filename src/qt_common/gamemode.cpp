// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// While technically available on al *NIX platforms, Linux is only available
// as the primary target of libgamemode.so - so warnings are suppressed
#ifdef __unix__
#include <gamemode_client.h>
#endif
#include "qt_common/gamemode.h"
#include "common/logging/log.h"
#include "qt_common/config/uisettings.h"

namespace Common::FeralGamemode {

/// @brief Start the gamemode client
void Start() noexcept {
    if (UISettings::values.enable_gamemode) {
#ifdef __unix__
        if (gamemode_request_start() < 0) {
#ifdef __linux__
            LOG_WARNING(Frontend, "{}", gamemode_error_string());
#else
            LOG_INFO(Frontend, "{}", gamemode_error_string());
#endif
        } else {
            LOG_INFO(Frontend, "Done");
        }
#endif
    }
}

/// @brief Stop the gmemode client
void Stop() noexcept {
    if (UISettings::values.enable_gamemode) {
#ifdef __unix__
        if (gamemode_request_end() < 0) {
#ifdef __linux__
            LOG_WARNING(Frontend, "{}", gamemode_error_string());
#else
            LOG_INFO(Frontend, "{}", gamemode_error_string());
#endif
        } else {
            LOG_INFO(Frontend, "Done");
        }
#endif
    }
}

} // namespace Common::Linux
