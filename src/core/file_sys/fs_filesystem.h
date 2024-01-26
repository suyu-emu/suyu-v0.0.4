// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"

namespace FileSys {

enum class OpenMode : u32 {
    Read = (1 << 0),
    Write = (1 << 1),
    AllowAppend = (1 << 2),

    ReadWrite = (Read | Write),
    All = (ReadWrite | AllowAppend),
};
DECLARE_ENUM_FLAG_OPERATORS(OpenMode)

enum class OpenDirectoryMode : u64 {
    Directory = (1 << 0),
    File = (1 << 1),

    All = (Directory | File),
};
DECLARE_ENUM_FLAG_OPERATORS(OpenDirectoryMode)

enum class DirectoryEntryType : u8 {
    Directory = 0,
    File = 1,
};

enum class CreateOption : u8 {
    None = (0 << 0),
    BigFile = (1 << 0),
};

} // namespace FileSys
