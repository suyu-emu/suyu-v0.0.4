// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <string_view>

#include <fmt/ranges.h>

#include "common/logging/formatter.h"
#include "common/logging/types.h"

namespace Common::Log {

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename,
                       unsigned int line_num, const char* function, fmt::string_view format,
                       const fmt::format_args& args);

template <typename... Args>
void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num,
                   const char* function, fmt::format_string<Args...> format, const Args&... args) {
    FmtLogMessageImpl(log_class, log_level, filename, line_num, function, format,
                      fmt::make_format_args(args...));
}

} // namespace Common::Log

#ifdef _DEBUG
#define LOG_TRACE(log_class, ...)                                                                  \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Trace,           \
                               __FILE__, __LINE__, __func__,          \
                               __VA_ARGS__)
#else
#define LOG_TRACE(log_class, fmt, ...) (void(0))
#endif

#define LOG_DEBUG(log_class, ...)                                                                  \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Debug,           \
                               __FILE__, __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_INFO(log_class, ...)                                                                   \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Info,            \
                               __FILE__, __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_WARNING(log_class, ...)                                                                \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Warning,         \
                               __FILE__, __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_ERROR(log_class, ...)                                                                  \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Error,           \
                               __FILE__, __LINE__, __func__,          \
                               __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...)                                                               \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Critical,        \
                               __FILE__, __LINE__, __func__,          \
                               __VA_ARGS__)
