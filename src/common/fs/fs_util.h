// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <concepts>
#include <string>
#include <string_view>

namespace Common::FS {

template <typename T>
concept IsChar = std::same_as<T, char>;

/**
 * Converts a UTF-8 encoded std::string or std::string_view to a std::u8string.
 *
 * @param utf8_string UTF-8 encoded string
 *
 * @returns UTF-8 encoded std::u8string.
 */
[[nodiscard]] std::u8string ToU8String(std::string_view utf8_string);

} // namespace Common::FS
