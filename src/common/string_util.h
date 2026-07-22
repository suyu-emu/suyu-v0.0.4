// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2013 Dolphin Emulator Project
// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <cstddef>
#include <span>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Common {

/// Make a string lowercase
[[nodiscard]] std::string ToLower(const std::string_view sv);

/// Make a string uppercase
[[nodiscard]] std::string ToUpper(const std::string_view sv);

[[nodiscard]] inline std::string StringFromBuffer(std::span<const u8> data) noexcept {
    return std::string(data.begin(), std::find(data.begin(), data.end(), '\0'));
}
[[nodiscard]] inline std::string StringFromBuffer(std::span<const char> data) noexcept {
    return std::string(data.begin(), std::find(data.begin(), data.end(), '\0'));
}

/// Turns "  hej " into "hej". Also handles tabs.
[[nodiscard]] inline std::string StripSpaces(const std::string_view str) noexcept {
    const std::size_t s = str.find_first_not_of(" \t\r\n");
    if (str.npos != s)
        return std::string{str.substr(s, str.find_last_not_of(" \t\r\n") - s + 1)};
    return {};
}

/// "\"hello\"" is turned to "hello"
/// This one assumes that the string has already been space stripped in both
/// ends, as done by StripSpaces above, for example.
[[nodiscard]] inline std::string StripQuotes(const std::string_view s) noexcept {
    if (s.size() && '\"' == s[0] && '\"' == *s.rbegin())
        return std::string{s.substr(1, s.size() - 2)};
    return std::string{s};
}

[[nodiscard]] inline std::string StringFromBool(bool value) noexcept {
    return value ? "True" : "False";
}

[[nodiscard]] std::string TabsToSpaces(int tab_size, std::string in);

void SplitString(const std::string& str, char delim, std::vector<std::string>& output);

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename,
               std::string* _pExtension);

[[nodiscard]] std::string ReplaceAll(std::string result, const std::string& src,
                                     const std::string& dest);

[[nodiscard]] std::string UTF16ToUTF8(std::u16string_view input);
[[nodiscard]] std::u16string UTF8ToUTF16(std::string_view input);
[[nodiscard]] std::u32string UTF8ToUTF32(std::string_view input);

#ifdef _WIN32
[[nodiscard]] std::string UTF16ToUTF8(std::wstring_view input);
[[nodiscard]] std::wstring UTF8ToUTF16W(std::string_view str);

#endif

[[nodiscard]] std::u16string U16StringFromBuffer(const u16* input, std::size_t length);

/**
 * Compares the string defined by the range [`begin`, `end`) to the null-terminated C-string
 * `other` for equality.
 */
template <typename InIt>
[[nodiscard]] inline bool ComparePartialString(InIt begin, InIt end, const char* other) noexcept {
    for (; begin != end && *other != '\0'; ++begin, ++other) {
        if (*begin != *other) {
            return false;
        }
    }
    // Only return true if both strings finished at the same point
    return (begin == end) == (*other == '\0');
}

/// Creates a std::string from a fixed-size NUL-terminated char buffer. If the buffer isn't
/// NUL-terminated then the string ends at max_len characters.
[[nodiscard]] std::string StringFromFixedZeroTerminatedBuffer(std::string_view buffer,
                                                              std::size_t max_len);

/// Creates a UTF-16 std::u16string from a fixed-size NUL-terminated char buffer. If the buffer isn't
/// null-terminated, then the string ends at the greatest multiple of two less then or equal to
/// max_len_bytes.
[[nodiscard]] std::u16string UTF16StringFromFixedZeroTerminatedBuffer(std::u16string_view buffer,
                                                                      std::size_t max_len);

} // namespace Common
