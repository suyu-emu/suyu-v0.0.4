// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/common_types.h"
#include <filesystem>
#include <vector>

namespace Common::FS {

constexpr const char IMEN_MAGIC[4] = {0x49, 0x4d, 0x45, 0x4e};
constexpr const char IMKV_MAGIC[4] = {0x49, 0x4d, 0x4b, 0x56};
constexpr const u8 IMEN_SIZE = 0x8c;

std::filesystem::path GetKvdbPath();
std::filesystem::path GetKvdbPath(const std::filesystem::path &path);
std::filesystem::path GetRyuPathFromSavePath(const std::filesystem::path &path);
std::filesystem::path GetRyuSavePath(const u64 &save_id);
std::filesystem::path GetRyuSavePath(const std::filesystem::path &path, const u64 &save_id);

enum class IMENReadResult {
    Nonexistent,  // ryujinx not found
    NoHeader,     // file isn't big enough for header
    InvalidMagic, // no IMKV or IMEN header
    Misaligned,   // file isn't aligned to expected IMEN boundaries
    NoImens,      // no-op, there are no IMENs
    Success,      // :)
};

struct IMEN
{
    u64 title_id;
    u64 save_id;
};

static_assert(sizeof(IMEN) == 0x10, "IMEN has incorrect size.");

IMENReadResult ReadKvdb(const std::filesystem::path &path, std::vector<IMEN> &imens);

} // namespace Common::FS
