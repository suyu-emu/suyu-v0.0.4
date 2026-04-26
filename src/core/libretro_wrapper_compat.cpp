// SPDX-FileCopyrightText: Copyright 2026 SuyuEclipse Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"

#include "common/logging/log.h"

namespace Core {

LibretroWrapper::LibretroWrapper() : core_handle(nullptr), game_info{} {}

LibretroWrapper::~LibretroWrapper() {
    Unload();
}

bool LibretroWrapper::LoadCore(const std::string& core_path) {
    LOG_WARNING(Core, "Libretro core loading is not available in this build: {}", core_path);
    return false;
}

bool LibretroWrapper::LoadGame(const std::string& game_path) {
    LOG_WARNING(Core, "Libretro game loading is not available in this build: {}", game_path);
    return false;
}

void LibretroWrapper::Run() {}

void LibretroWrapper::Reset() {}

void LibretroWrapper::Unload() {
    core_handle = nullptr;
    game_info = {};
}

} // namespace Core
