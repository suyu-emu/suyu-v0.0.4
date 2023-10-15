// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "common/fs/fs_util.h"
#include "common/polyfill_ranges.h"

namespace Common::FS {

std::u8string ToU8String(std::string_view utf8_string) {
    return std::u8string{utf8_string.begin(), utf8_string.end()};
}

std::u8string BufferToU8String(std::span<const u8> buffer) {
    return std::u8string{buffer.begin(), std::ranges::find(buffer, u8{0})};
}

std::u8string_view BufferToU8StringView(std::span<const u8> buffer) {
    return std::u8string_view{reinterpret_cast<const char8_t*>(buffer.data())};
}

std::string ToUTF8String(std::u8string_view u8_string) {
    return std::string{u8_string.begin(), u8_string.end()};
}

std::string BufferToUTF8String(std::span<const u8> buffer) {
    return std::string{buffer.begin(), std::ranges::find(buffer, u8{0})};
}

std::string_view BufferToUTF8StringView(std::span<const u8> buffer) {
    return std::string_view{reinterpret_cast<const char*>(buffer.data())};
}

std::string PathToUTF8String(const std::filesystem::path& path) {
    return ToUTF8String(path.u8string());
}

std::u8string U8FilenameSanitizer(const std::u8string_view u8filename) {
    std::u8string u8path_sanitized{u8filename.begin(), u8filename.end()};
    size_t eSizeSanitized = u8path_sanitized.size();

    // The name is improved to make it look more beautiful and prohibited characters and shapes are
    // removed. Switch is used since it is better with many conditions.
    for (size_t i = 0; i < eSizeSanitized; i++) {
        switch (u8path_sanitized[i]) {
        case u8':':
            if (i == 0 || i == eSizeSanitized - 1) {
                u8path_sanitized.replace(i, 1, u8"_");
            } else if (u8path_sanitized[i - 1] == u8' ') {
                u8path_sanitized.replace(i, 1, u8"-");
            } else {
                u8path_sanitized.replace(i, 1, u8" -");
                eSizeSanitized++;
            }
            break;
        case u8'\\':
        case u8'/':
        case u8'*':
        case u8'?':
        case u8'\"':
        case u8'<':
        case u8'>':
        case u8'|':
        case u8'\0':
            u8path_sanitized.replace(i, 1, u8"_");
            break;
        default:
            break;
        }
    }

    // Delete duplicated spaces and dots
    for (size_t i = 0; i < eSizeSanitized - 1; i++) {
        if ((u8path_sanitized[i] == u8' ' && u8path_sanitized[i + 1] == u8' ') ||
            (u8path_sanitized[i] == u8'.' && u8path_sanitized[i + 1] == u8'.')) {
            u8path_sanitized.erase(i, 1);
            i--;
        }
    }

    // Delete all spaces and dots at the end of the name
    while (u8path_sanitized.back() == u8' ' || u8path_sanitized.back() == u8'.') {
        u8path_sanitized.pop_back();
    }

    if (u8path_sanitized.empty()) {
        return u8"";
    }

    return u8path_sanitized;
}

std::string UTF8FilenameSanitizer(const std::string_view filename) {
    return ToUTF8String(U8FilenameSanitizer(ToU8String(filename)));
}

} // namespace Common::FS
