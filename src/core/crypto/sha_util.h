// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once
#include <array>
#include <cstddef>
#include "common/common_types.h"

namespace Crypto {
typedef std::array<u8, 0x20> SHA256Hash;

// User-defined literal operator to convert hex strings to SHA256Hash
constexpr SHA256Hash operator""_HASH(const char* data, std::size_t len) {
    if (len != 0x40) {
        return {};
    }

    // Validate that all characters are valid hex characters
    for (std::size_t i = 0; i < len; ++i) {
        char c = data[i];
        const bool is_valid_hex = (c >= '0' && c <= '9') ||
                           (c >= 'a' && c <= 'f') ||
                           (c >= 'A' && c <= 'F');
        if (!is_valid_hex) {
            return {};
        }
    }

    // Convert hex string to array manually to avoid potential assertion issues
    // Each pair of hex characters becomes one byte
    SHA256Hash result{};
    for (std::size_t i = 0; i < len; i += 2) {
        const u8 high_nibble = (data[i] >= '0' && data[i] <= '9') ? (data[i] - '0') :
                        (data[i] >= 'a' && data[i] <= 'f') ? (data[i] - 'a' + 10) :
                        (data[i] - 'A' + 10);
        const u8 low_nibble = (data[i + 1] >= '0' && data[i + 1] <= '9') ? (data[i + 1] - '0') :
                       (data[i + 1] >= 'a' && data[i + 1] <= 'f') ? (data[i + 1] - 'a' + 10) :
                       (data[i + 1] - 'A' + 10);
        result[i / 2] = (high_nibble << 4) | low_nibble;
    }
    return result;
}

} // namespace Crypto
