// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlink.h"

#ifdef _WIN32
#include <windows.h>
#include <fmt/format.h>
#endif

namespace fs = std::filesystem;

// The sole purpose of this file is to treat symlinks like symlinks on POSIX,
// or treat them as directory junctions on Windows.
// This is because, for some inexplicable reason, Microsoft has locked symbolic
// links behind a "security policy", whereas directory junctions--functionally identical
// for directories, by the way--are not. Why? I don't know.

namespace Common::FS {

bool CreateSymlink(const fs::path &from, const fs::path &to)
{
#ifdef _WIN32
    const std::string command = fmt::format("mklink /J {} {}", to.string(), from.string());
    return system(command.c_str()) == 0;
#else
    std::error_code ec;
    fs::create_directory_symlink(from, to, ec);
    return !ec;
#endif
}

bool IsSymlink(const fs::path &path)
{
#ifdef _WIN32
    auto attributes = GetFileAttributesW(path.wstring().c_str());
    return attributes & FILE_ATTRIBUTE_REPARSE_POINT;
#else
    return fs::is_symlink(path);
#endif
}

} // namespace Common::FS
