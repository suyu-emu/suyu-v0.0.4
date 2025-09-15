// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include "common/hex_util.h"

namespace Crypto {
typedef std::array<u8, 0x20> SHA256Hash;

inline SHA256Hash operator"" _HASH(const char* data, size_t len) {
    if (len != 0x40) {
        return {};
    }

    // Validate that all characters are valid hex characters
    for (size_t i = 0; i < len; ++i) {
        char c = data[i];
        bool is_valid_hex = (c >= '0' && c <= '9') ||
                           (c >= 'a' && c <= 'f') ||
                           (c >= 'A' && c <= 'F');
        if (!is_valid_hex) {
            return {};
        }
    }

    // Only call HexStringToArray if we've validated the input
    std::string_view hex_view(data, len);
    return Common::HexStringToArray<0x20>(hex_view);
}

} // namespace Crypto
