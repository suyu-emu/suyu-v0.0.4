// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "ryujinx_compat.h"
#include "common/fs/path_util.h"
#include <cstddef>
#include <cstring>
#include <fmt/ranges.h>
#include <fstream>

namespace Common::FS {

namespace fs = std::filesystem;

fs::path GetKvdbPath()
{
    return GetKvdbPath(GetLegacyPath(EmuPath::RyujinxDir));
}

fs::path GetKvdbPath(const fs::path& path) {
    return path / "bis" / "system" / "save" / "8000000000000000" / "0"
           / "imkvdb.arc";
}

fs::path GetRyuPathFromSavePath(const fs::path& path) {
    // This is a horrible hack, but I cba to find something better
    return path.parent_path().parent_path().parent_path().parent_path().parent_path();
}

fs::path GetRyuSavePath(const u64 &save_id)
{
    return GetRyuSavePath(GetLegacyPath(EmuPath::RyujinxDir), save_id);
}

std::filesystem::path GetRyuSavePath(const std::filesystem::path& path, const u64& save_id) {
    std::string hex = fmt::format("{:016x}", save_id);

           // TODO: what's the difference between 0 and 1?
    return path / "bis" / "user" / "save" / hex / "0";
}

IMENReadResult ReadKvdb(const fs::path &path, std::vector<IMEN> &imens)
{
    std::ifstream kvdb{path, std::ios::binary | std::ios::ate};

    if (!kvdb) {
        return IMENReadResult::Nonexistent;
    }

    size_t file_size = kvdb.tellg();

    // IMKV header + 8 bytes
    if (file_size < 0xB) {
        return IMENReadResult::NoHeader;
    }

    // magic (not the wizard kind)
    kvdb.seekg(0, std::ios::beg);
    char header[12];
    kvdb.read(header, 12);

    if (std::memcmp(header, IMKV_MAGIC, 4) != 0) {
        return IMENReadResult::InvalidMagic;
    }

    // calculate num. of imens left
    std::size_t remaining = (file_size - 12);
    std::size_t num_imens = remaining / IMEN_SIZE;

    // File is misaligned and probably corrupt (rip)
    if (remaining % IMEN_SIZE != 0) {
        return IMENReadResult::Misaligned;
    }

    // if there aren't any IMENs, it's empty and we can safely no-op out of here
    if (num_imens == 0) {
        return IMENReadResult::NoImens;
    }

    imens.reserve(num_imens);

    // initially I wanted to do a struct, but imkvdb is 140 bytes
    // while the compiler will murder you if you try to align u64 to 4 bytes
    for (std::size_t i = 0; i < num_imens; ++i) {
        char magic[4];
        u64 title_id = 0;
        u64 save_id = 0;

        kvdb.read(magic, 4);
        if (std::memcmp(magic, IMEN_MAGIC, 4) != 0) {
            return IMENReadResult::InvalidMagic;
        }

        kvdb.ignore(0x8);
        kvdb.read(reinterpret_cast<char *>(&title_id), 8);
        kvdb.ignore(0x38);
        kvdb.read(reinterpret_cast<char *>(&save_id), 8);
        kvdb.ignore(0x38);

        imens.emplace_back(IMEN{title_id, save_id});
    }

    return IMENReadResult::Success;
}

} // namespace Common::FS
