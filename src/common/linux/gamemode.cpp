// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef HAVE_GAMEMODE_CLIENT_H
#include <gamemode_client.h>
#endif

#include "common/linux/gamemode.h"
#include "common/logging/log.h"
#include "common/settings.h"

namespace Common::Linux {

void StartGamemode() {
#ifdef HAVE_GAMEMODE_CLIENT_H
    if (Settings::values.enable_gamemode) {
        if (gamemode_request_start() < 0) {
            LOG_WARNING(Frontend, "Failed to start gamemode: {}", gamemode_error_string());
        } else {
            LOG_INFO(Frontend, "Started gamemode");
        }
    }
#endif
}

void StopGamemode() {
#ifdef HAVE_GAMEMODE_CLIENT_H
    if (Settings::values.enable_gamemode) {
        if (gamemode_request_end() < 0) {
            LOG_WARNING(Frontend, "Failed to stop gamemode: {}", gamemode_error_string());
        } else {
            LOG_INFO(Frontend, "Stopped gamemode");
        }
    }
#endif
}

void SetGamemodeState(bool state) {
    if (state) {
        StartGamemode();
    } else {
        StopGamemode();
    }
}

} // namespace Common::Linux
