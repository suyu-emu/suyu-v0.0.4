// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <chrono>
#include <algorithm>
#include <type_traits>
#include <fmt/ranges.h>
#include "common/swap.h"

// adapted from https://github.com/fmtlib/fmt/issues/2704
// a generic formatter for enum classes
#if FMT_VERSION >= 80100
template <typename T>
struct fmt::formatter<T, std::enable_if_t<std::is_enum_v<T>, char>>
    : formatter<std::underlying_type_t<T>> {
    template <typename FormatContext>
    auto format(const T& value, FormatContext& ctx) const -> decltype(ctx.out()) {
        return fmt::formatter<std::underlying_type_t<T>>::format(
            static_cast<std::underlying_type_t<T>>(value), ctx);
    }
};
#endif

template <typename T, typename U>
struct fmt::formatter<SwapStructT<T, U>> {
    constexpr auto parse(format_parse_context& ctx) {
        return ctx.begin();
    }
    template <typename FormatContext>
    auto format(const SwapStructT<T, U>& reg, FormatContext& ctx) const {
        return fmt::format_to(ctx.out(), "{}", T(reg));
    }
};

#ifdef _DEBUG
#define LOG_TRACE(log_class, ...) \
    Common::Log::FmtLogMessage(Common::Log::Class::log_class, Common::Log::Level::Trace, \
       __FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define LOG_TRACE(log_class, fmt, ...) (void(0))
#endif

#define LOG_DEBUG(log_class, ...) \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Debug, \
       __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_INFO(log_class, ...) \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Info, \
       __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_WARNING(log_class, ...) \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Warning, \
       __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_ERROR(log_class, ...) \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Error, \
       __FILE__, __LINE__, __func__, __VA_ARGS__)
#define LOG_CRITICAL(log_class, ...) \
    ::Common::Log::FmtLogMessage(::Common::Log::Class::log_class, ::Common::Log::Level::Critical, \
       __FILE__, __LINE__, __func__, __VA_ARGS__)

namespace Common::Log {

/// Specifies the severity or level of detail of the log message.
enum class Level : u8 {
    Trace, ///< Extremely detailed and repetitive debugging information that is likely to pollute logs.
    Debug, ///< Less detailed debugging information.
    Info, ///< Status information from important points during execution.
    Warning, ///< Minor or potential problems found during execution of a task.
    Error, ///< Major problems found during execution of a task that prevent it from being completed.
    Critical, ///< Major problems during execution that threaten the stability of the entire application.
    Count ///< Total number of logging levels
};

/// Specifies the sub-system that generated the log message.
enum class Class : u8 {
#define SUB(A, B) A##_##B,
#define CLS(A) A,
#include "log_classes.inc"
#undef SUB
#undef CLS
    Count,
};

/// Logs a message to the global logger, using fmt
void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename, unsigned int line_num, const char* function, fmt::string_view format, const fmt::format_args& args);

template <typename... Args>
void FmtLogMessage(Class log_class, Level log_level, const char* filename, unsigned int line_num, const char* function, fmt::format_string<Args...> format, const Args&... args) {
    FmtLogMessageImpl(log_class, log_level, filename, line_num, function, format, fmt::make_format_args(args...));
}

/// Implements a log message filter which allows different log classes to have different minimum
/// severity levels. The filter can be changed at runtime and can be parsed from a string to allow
/// editing via the interface or loading from a configuration file.
struct Filter {
    /// Initializes the filter with all classes having `default_level` as the minimum level.
    explicit Filter(Level level = Level::Info) {
        class_levels.fill(level);
    }
    /// Sets the minimum level of `log_class` (and not of its subclasses) to `level`.
    void SetClassLevel(Class log_class, Level level) {
        class_levels[std::size_t(log_class)] = level;
    }
    /// Parses a filter string and applies it to this filter.
    /// A filter string consists of a space-separated list of filter rules, each of the format
    /// `<class>:<level>`. `<class>` is a log class name, with subclasses separated using periods.
    /// `*` is allowed as a class name and will reset all filters to the specified level. `<level>`
    /// a severity level name which will be set as the minimum logging level of the matched classes.
    /// Rules are applied left to right, with each rule overriding previous ones in the sequence.
    /// A few examples of filter rules:
    ///  - `*:Info` -- Resets the level of all classes to Info.
    ///  - `Service:Info` -- Sets the level of Service to Info.
    ///  - `Service.FS:Trace` -- Sets the level of the Service.FS class to Trace.
    void ParseFilterString(std::string_view filter_view);
    /// Matches class/level combination against the filter, returning true if it passed.
    [[nodiscard]] bool CheckMessage(Class log_class, Level level) const {
        return u8(level) >= u8(class_levels[std::size_t(log_class)]);
    }
    /// Returns true if any logging classes are set to debug
    [[nodiscard]] bool IsDebug() const {
        return std::any_of(class_levels.begin(), class_levels.end(), [](const Level& l) {
            return u8(l) <= u8(Level::Debug);
        });
    }
    std::array<Level, std::size_t(Class::Count)> class_levels;
};

/// Initializes the logging system. This should be the first thing called in main.
void Initialize();

void Start();

/// Explicitly stops the logger thread and flushes the buffers
void Stop();

/// The global filter will prevent any messages from even being processed if they are filtered.
void SetGlobalFilter(const Filter& filter);
void SetColorConsoleBackendEnabled(bool enabled);

/// @brief A log entry. Log entries are store in a structured format to permit more varied output
/// formatting on different frontends, as well as facilitating filtering and aggregation.
struct Entry {
    std::string message;
    std::chrono::microseconds timestamp;
    Class log_class{};
    Level log_level{};
    const char* filename = nullptr;
    const char* function = nullptr;
    unsigned int line_num = 0;
};

/// Formats a log entry into the provided text buffer.
std::string FormatLogMessage(const Entry& entry) noexcept;

/// Prints the same message as `PrintMessage`, but colored according to the severity level.
void PrintColoredMessage(const Entry& entry) noexcept;

/// Formats and prints a log entry to the android logcat.
void PrintMessageToLogcat(const Entry& entry) noexcept;

} // namespace Common::Log
