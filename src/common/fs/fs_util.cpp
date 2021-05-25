// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/fs/fs_util.h"

namespace Common::FS {

std::u8string ToU8String(std::string_view utf8_string) {
    return std::u8string{utf8_string.begin(), utf8_string.end()};
}

} // namespace Common::FS
