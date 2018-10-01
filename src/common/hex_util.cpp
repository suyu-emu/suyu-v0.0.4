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

std::vector<u8> HexStringToVector(std::string_view str, bool little_endian) {
    std::vector<u8> out(str.size() / 2);
    if (little_endian) {
        for (std::size_t i = str.size() - 2; i <= str.size(); i -= 2)
            out[i / 2] = (ToHexNibble(str[i]) << 4) | ToHexNibble(str[i + 1]);
    } else {
        for (std::size_t i = 0; i < str.size(); i += 2)
            out[i / 2] = (ToHexNibble(str[i]) << 4) | ToHexNibble(str[i + 1]);
    }
    return out;
}

std::string HexVectorToString(const std::vector<u8>& vector, bool upper) {
    std::string out;
    for (u8 c : vector)
        out += fmt::format(upper ? "{:02X}" : "{:02x}", c);
    return out;
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
