// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef DATA_MANAGER_H
#define DATA_MANAGER_H

#include "common/common_types.h"
#include <string>
#include <filesystem>

namespace FrontendCommon::DataManager {

enum class DataDir { Saves, UserNand, SysNand, Mods, Shaders };

const std::filesystem::path GetDataDir(DataDir dir, const std::string &user_id = "");
const std::string GetDataDirString(DataDir dir, const std::string &user_id = "");

u64 ClearDir(DataDir dir, const std::string &user_id = "");

const std::string ReadableBytesSize(u64 size);

u64 DataDirSize(DataDir dir);

}; // namespace FrontendCommon::DataManager

#endif // DATA_MANAGER_H
