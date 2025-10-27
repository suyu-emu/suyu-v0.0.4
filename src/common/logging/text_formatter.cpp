// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <array>
#include <cstdio>
#include <cstdint>
#include <string>

#ifdef _WIN32
#include <windows.h>
#elif defined(__ANDROID__)
#include <android/log.h>
#endif

#include "common/assert.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/logging/log_entry.h"
#include "common/logging/text_formatter.h"

namespace Common::Log {

std::string FormatLogMessage(const Entry& entry) {
    auto const time_seconds = uint32_t(entry.timestamp.count() / 1000000);
    auto const time_fractional = uint32_t(entry.timestamp.count() % 1000000);
    char const* class_name = GetLogClassName(entry.log_class);
    char const* level_name = GetLevelName(entry.log_level);
    return fmt::format("[{:4d}.{:06d}] {} <{}> {}:{}:{}: {}", time_seconds, time_fractional,
                       class_name, level_name, entry.filename, entry.function, entry.line_num,
                       entry.message);
}

void PrintMessage(const Entry& entry) {
#ifdef _WIN32
    auto const str = FormatLogMessage(entry).append(1, '\n');
#else
#define ESC "\x1b"
    auto str = std::string{[&entry]() -> const char* {
        switch (entry.log_level) {
        case Level::Debug: return ESC "[0;36m"; // Cyan
        case Level::Info: return ESC "[0;37m"; // Bright gray
        case Level::Warning: return ESC "[1;33m"; // Bright yellow
        case Level::Error: return ESC "[1;31m"; // Bright red
        case Level::Critical: return ESC "[1;35m"; // Bright magenta
        default: return ESC "[1;30m"; // Grey
        }
    }()};
    str.append(FormatLogMessage(entry));
    str.append(ESC "[0m\n");
#undef ESC
#endif
    fwrite(str.c_str(), 1, str.size(), stderr);
}

void PrintColoredMessage(const Entry& entry) {
#ifdef _WIN32
    HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);
    if (console_handle == INVALID_HANDLE_VALUE)
        return;
    CONSOLE_SCREEN_BUFFER_INFO original_info = {};
    GetConsoleScreenBufferInfo(console_handle, &original_info);
    WORD color = WORD([&entry]() {
        switch (entry.log_level) {
        case Level::Debug: return FOREGROUND_GREEN | FOREGROUND_BLUE; // Cyan
        case Level::Info: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE; // Bright gray
        case Level::Warning: return FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        case Level::Error: return FOREGROUND_RED | FOREGROUND_INTENSITY;
        case Level::Critical: return FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        default: break;
        }
        return FOREGROUND_INTENSITY; // Grey
    }());
    SetConsoleTextAttribute(console_handle, color);
#endif
    PrintMessage(entry);
#ifdef _WIN32
    SetConsoleTextAttribute(console_handle, original_info.wAttributes);
#endif
}

void PrintMessageToLogcat(const Entry& entry) {
#ifdef ANDROID
    android_LogPriority android_log_priority = [&]() {
        switch (entry.log_level) {
        case Level::Debug: return ANDROID_LOG_DEBUG;
        case Level::Info: return ANDROID_LOG_INFO;
        case Level::Warning: return ANDROID_LOG_WARN;
        case Level::Error: return ANDROID_LOG_ERROR;
        case Level::Critical: return ANDROID_LOG_FATAL;
        case Level::Count:
        case Level::Trace: return ANDROID_LOG_VERBOSE;
        }
    }();
    auto const str = FormatLogMessage(entry);
    __android_log_print(android_log_priority, "YuzuNative", "%s", str.c_str());
#endif
}
} // namespace Common::Log
