// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// Copyright Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>

namespace UpdateChecker {
std::optional<std::string> GetResponse(std::string url, std::string path);
std::optional<std::string> GetLatestRelease(bool include_prereleases);
} // namespace UpdateChecker
