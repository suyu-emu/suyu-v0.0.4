// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "symlink.h"

#ifdef _WIN32
#include <fmt/format.h>
#include <windows.h>
#endif

#include <boost/filesystem.hpp>

namespace fs = std::filesystem;

// The sole purpose of this file is to treat symlinks like symlinks on POSIX,
// or treat them as directory junctions on Windows.
// This is because, for some inexplicable reason, Microsoft has locked symbolic
// links behind a "security policy", whereas directory junctions--functionally identical
// for directories, by the way--are not. Why? I don't know.
// And no, they do NOT provide a standard API for this (at least to my knowledge).
// CreateSymbolicLink, even when EXPLICITLY TOLD to create a junction, still fails
// because of their security policy.
// I don't know what kind of drugs the Windows developers have been on since NT started.

// Microsoft still has not implemented any of this in their std::filesystem implemenation,
// which ALSO means that it DOES NOT FOLLOW ANY DIRECTORY JUNCTIONS... AT ALL.
// Nor does any of their command line utilities or APIs. So you're quite literally
// on your own.

namespace Common::FS {

bool CreateSymlink(fs::path from, fs::path to)
{
    from.make_preferred();
    to.make_preferred();

    std::error_code ec;
    fs::create_directory_symlink(from, to, ec);
#ifdef _WIN32
    if (ec) {
        const std::string command = fmt::format("mklink /J \"{}\" \"{}\"",
                                                to.string(),
                                                from.string());
        return system(command.c_str()) == 0;
    }
#endif

    return !ec;
}

bool IsSymlink(const fs::path &path)
{
    return boost::filesystem::is_symlink(boost::filesystem::path{path});
}

} // namespace Common::FS
