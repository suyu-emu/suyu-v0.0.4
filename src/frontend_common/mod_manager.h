// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <filesystem>
#include <optional>
#include "common/common_types.h"

namespace FrontendCommon {

enum ModInstallResult {
    Cancelled,
    Failed,
    Success,
};

std::vector<std::filesystem::path> GetModFolder(const std::string& root);

ModInstallResult InstallMod(const std::filesystem::path &path, const u64 program_id, const bool copy = true);
}
