// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/hex_util.h"
#include "common/logging/log.h"

namespace Common {

u8 ToHexNibble(char c1) {
    if (c1 >= 65 && c1 <= 70)
        return c1 - 55;
    if (c1 >= 97 && c1 <= 102)
        return c1 - 87;
    if (c1 >= 48 && c1 <= 57)
        return c1 - 48;
    LOG_ERROR(Common, "Invalid hex digit: 0x{:02X}", c1);
    return 0;
}

std::array<u8, 16> operator""_array16(const char* str, std::size_t len) {
    if (len != 32) {
        LOG_ERROR(Common,
                  "Attempting to parse string to array that is not of correct size (expected=32, "
                  "actual={}).",
                  len);
        return {};
    }
    return HexStringToArray<16>(str);
}

std::array<u8, 32> operator""_array32(const char* str, std::size_t len) {
    if (len != 64) {
        LOG_ERROR(Common,
                  "Attempting to parse string to array that is not of correct size (expected=64, "
                  "actual={}).",
                  len);
        return {};
    }
    return HexStringToArray<32>(str);
}

} // namespace Common
