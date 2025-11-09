// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/common_types.h"
#include <filesystem>
#include <optional>

#pragma once

namespace QtCommon::FS {

void LinkRyujinx(std::filesystem::path &from, std::filesystem::path &to);
const std::filesystem::path GetRyujinxSavePath(const std::filesystem::path &path_hint, const u64 &program_id);

/// returns FALSE if the dirs are NOT linked
bool CheckUnlink(const std::filesystem::path& eden_dir,
                 const std::filesystem::path& ryu_dir);

} // namespace QtCommon::FS
