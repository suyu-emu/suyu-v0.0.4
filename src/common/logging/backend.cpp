// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <atomic>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <regex>
#include <thread>

#include <boost/algorithm/string/replace.hpp>
#include <fmt/ranges.h>

#ifdef _WIN32
#include <windows.h> // For OutputDebugStringW
#endif

#include "common/fs/file.h"
#include "common/fs/fs.h"
#include "common/fs/fs_paths.h"
#include "common/fs/path_util.h"
#include "common/literals.h"
#include "common/polyfill_thread.h"
#include "common/thread.h"

#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/log_entry.h"
#include "common/logging/text_formatter.h"
#include "common/settings.h"
#ifdef _WIN32
#include "common/string_util.h"
#endif
#include "common/bounded_threadsafe_queue.h"

namespace Common::Log {

namespace {

/// @brief Trims up to and including the last of ../, ..\, src/, src\ in a string
/// do not be fooled this isn't generating new strings on .rodata :)
constexpr const char* TrimSourcePath(std::string_view source) noexcept {
    const auto rfind = [source](const std::string_view match) {
        return source.rfind(match) == source.npos ? 0 : (source.rfind(match) + match.size());
    };
    auto idx = (std::max)({rfind("src/"), rfind("src\\"), rfind("../"), rfind("..\\")});
    return source.data() + idx;
}

/// @brief Interface for logging backends.
struct Backend {
    virtual ~Backend() noexcept = default;
    virtual void Write(const Entry& entry) noexcept = 0;
    virtual void EnableForStacktrace() noexcept= 0;
    virtual void Flush() noexcept = 0;
};

/// @brief Backend that writes to stderr and with color
struct ColorConsoleBackend final : public Backend {
    explicit ColorConsoleBackend() noexcept = default;
    ~ColorConsoleBackend() noexcept override = default;

    void Write(const Entry& entry) noexcept override {
        if (enabled.load(std::memory_order_relaxed))
            PrintColoredMessage(entry);
    }

    void Flush() noexcept override {
        // stderr shouldn't be buffered
    }

    void EnableForStacktrace() noexcept override {
        enabled = true;
    }

    void SetEnabled(bool enabled_) noexcept {
        enabled = enabled_;
    }

private:
    std::atomic_bool enabled{false};
};

#ifndef __OPENORBIS__
/// @brief Backend that writes to a file passed into the constructor
struct FileBackend final : public Backend {
    explicit FileBackend(const std::filesystem::path& filename) noexcept {
        auto old_filename = filename;
        old_filename += ".old.txt";

        // Existence checks are done within the functions themselves.
        // We don't particularly care if these succeed or not.
        void(FS::RemoveFile(old_filename));
        void(FS::RenameFile(filename, old_filename));

        file = std::make_unique<FS::IOFile>(filename, FS::FileAccessMode::Write, FS::FileType::TextFile);
    }

    ~FileBackend() noexcept override = default;

    void Write(const Entry& entry) noexcept override {
        if (!enabled)
            return;

        auto message = FormatLogMessage(entry).append(1, '\n');
#ifndef __ANDROID__
        if (Settings::values.censor_username.GetValue()) {
            // This must be a static otherwise it would get checked on EVERY
            // instance of logging an entry...
            static std::string username = []() -> std::string {
                // in order of precedence
                // LOGNAME usually works on UNIX, USERNAME on Windows
                // Some UNIX systems suck and don't use LOGNAME so we also
                // need USER :(
                for (auto const var : {
                         "LOGNAME",
                         "USERNAME",
                         "USER",
                     }) {
                    if (auto const s = getenv(var); s != nullptr)
                        return std::string{s};
                }

                return std::string{};
            }();
            if (!username.empty())
                boost::replace_all(message, username, "user");
        }
#endif
        bytes_written += file->WriteString(message);

        // Option to log each line rather than 4k buffers
        if (Settings::values.log_flush_line.GetValue())
            file->Flush();

        using namespace Common::Literals;
        // Prevent logs from exceeding a set maximum size in the event that log entries are spammed.
        const auto write_limit = Settings::values.extended_logging.GetValue() ? 1_GiB : 100_MiB;
        const bool write_limit_exceeded = bytes_written > write_limit;
        if (entry.log_level >= Level::Error || write_limit_exceeded) {
            // Stop writing after the write limit is exceeded.
            // Don't close the file so we can print a stacktrace if necessary
            if (write_limit_exceeded)
                enabled = false;
            file->Flush();
        }
    }

    void Flush() noexcept override {
        file->Flush();
    }

    void EnableForStacktrace() noexcept override {
        enabled = true;
        bytes_written = 0;
    }

private:
    std::unique_ptr<FS::IOFile> file;
    std::size_t bytes_written = 0;
    bool enabled = true;
};
#endif

#ifdef _WIN32
/// @brief Backend that writes to Visual Studio's output window
struct DebuggerBackend final : public Backend {
    explicit DebuggerBackend() noexcept = default;
    ~DebuggerBackend() noexcept override = default;
    void Write(const Entry& entry) noexcept override {
        ::OutputDebugStringW(UTF8ToUTF16W(FormatLogMessage(entry).append(1, '\n')).c_str());
    }
    void Flush() noexcept override {}
    void EnableForStacktrace() noexcept override {}
};
#endif
#ifdef ANDROID
/// @brief Backend that writes to the Android logcat
struct LogcatBackend : public Backend {
    explicit LogcatBackend() noexcept = default;
    ~LogcatBackend() noexcept override = default;
    void Write(const Entry& entry) noexcept override {
        PrintMessageToLogcat(entry);
    }
    void Flush() noexcept override {}
    void EnableForStacktrace() noexcept override {}
};
#endif

bool initialization_in_progress_suppress_logging = true;

/// @brief Static state as a singleton.
class Impl {
public:
    static Impl& Instance() noexcept {
        if (!instance)
            std::abort();
        return *instance;
    }

    static void Initialize() noexcept {
        if (instance) {
            LOG_WARNING(Log, "Reinitializing logging backend");
            return;
        }
        using namespace Common::FS;
        const auto& log_dir = GetEdenPath(EdenPath::LogDir);
        void(CreateDir(log_dir));
        Filter filter;
        filter.ParseFilterString(Settings::values.log_filter.GetValue());
        instance = std::unique_ptr<Impl, decltype(&Deleter)>(new Impl(log_dir / LOG_FILE, filter), Deleter);
        initialization_in_progress_suppress_logging = false;
    }

    static void Start() noexcept {
        instance->StartBackendThread();
    }

    static void Stop() noexcept {
        instance->StopBackendThread();
    }

    Impl(const Impl&) noexcept = delete;
    Impl& operator=(const Impl&) noexcept = delete;

    Impl(Impl&&) noexcept = delete;
    Impl& operator=(Impl&&) noexcept = delete;

    void SetGlobalFilter(const Filter& f) noexcept {
        filter = f;
    }

    void SetColorConsoleBackendEnabled(bool enabled) noexcept {
        color_console_backend.SetEnabled(enabled);
    }

    bool CanPushEntry(Class log_class, Level log_level) const noexcept {
        return filter.CheckMessage(log_class, log_level);
    }

    void PushEntry(Class log_class, Level log_level, const char* filename, unsigned int line_num, const char* function, std::string&& message) noexcept {
        message_queue.EmplaceWait(CreateEntry(log_class, log_level, TrimSourcePath(filename), line_num, function, std::move(message)));
    }

private:
    Impl(const std::filesystem::path& file_backend_filename, const Filter& filter_) noexcept :
        filter{filter_}
#ifndef __OPENORBIS__
        , file_backend{file_backend_filename}
#endif
    {}

    ~Impl() noexcept = default;

    void StartBackendThread() noexcept {
        backend_thread = std::jthread([this](std::stop_token stop_token) {
            Common::SetCurrentThreadName("Logger");
            Entry entry;
            const auto write_logs = [this, &entry]() {
                ForEachBackend([&entry](Backend& backend) {
                    backend.Write(entry);
                });
            };
            do {
                message_queue.PopWait(entry, stop_token);
                write_logs();
            } while (!stop_token.stop_requested());
            // Drain the logging queue. Only writes out up to MAX_LOGS_TO_WRITE to prevent a
            // case where a system is repeatedly spamming logs even on close.
            int max_logs_to_write = filter.IsDebug() ? INT_MAX : 100;
            while (max_logs_to_write-- && message_queue.TryPop(entry))
                write_logs();
        });
    }

    void StopBackendThread() noexcept {
        backend_thread.request_stop();
        if (backend_thread.joinable())
            backend_thread.join();
        ForEachBackend([](Backend& backend) { backend.Flush(); });
    }

    Entry CreateEntry(Class log_class, Level log_level, const char* filename, unsigned int line_nr, const char* function, std::string&& message) const noexcept {
        using std::chrono::duration_cast;
        using std::chrono::microseconds;
        using std::chrono::steady_clock;
        return {
            .timestamp = duration_cast<microseconds>(steady_clock::now() - time_origin),
            .log_class = log_class,
            .log_level = log_level,
            .filename = filename,
            .line_num = line_nr,
            .function = function,
            .message = std::move(message),
        };
    }

    void ForEachBackend(auto lambda) noexcept {
        lambda(static_cast<Backend&>(color_console_backend));
#ifndef __OPENORBIS__
        lambda(static_cast<Backend&>(file_backend));
#endif
#ifdef _WIN32
        lambda(static_cast<Backend&>(debugger_backend));
#endif
#ifdef ANDROID
        lambda(static_cast<Backend&>(lc_backend));
#endif
    }

    static void Deleter(Impl* ptr) noexcept {
        delete ptr;
    }

    static inline std::unique_ptr<Impl, decltype(&Deleter)> instance{nullptr, Deleter};

    Filter filter;
    ColorConsoleBackend color_console_backend{};
#ifndef __OPENORBIS__
    FileBackend file_backend;
#endif
#ifdef _WIN32
    DebuggerBackend debugger_backend{};
#endif
#ifdef ANDROID
    LogcatBackend lc_backend{};
#endif

    MPSCQueue<Entry> message_queue{};
    std::chrono::steady_clock::time_point time_origin{std::chrono::steady_clock::now()};
    std::jthread backend_thread;
};
} // namespace

void Initialize() {
    Impl::Initialize();
}

void Start() {
    Impl::Start();
}

void Stop() {
    Impl::Stop();
}

void DisableLoggingInTests() {
    initialization_in_progress_suppress_logging = true;
}

void SetGlobalFilter(const Filter& filter) {
    Impl::Instance().SetGlobalFilter(filter);
}

void SetColorConsoleBackendEnabled(bool enabled) {
    Impl::Instance().SetColorConsoleBackendEnabled(enabled);
}

void FmtLogMessageImpl(Class log_class, Level log_level, const char* filename, unsigned int line_num, const char* function, fmt::string_view format, const fmt::format_args& args) {
    if (!initialization_in_progress_suppress_logging) {
        auto& instance = Impl::Instance();
        if (instance.CanPushEntry(log_class, log_level))
            instance.PushEntry(log_class, log_level, filename, line_num, function, fmt::vformat(format, args));
    }
}
} // namespace Common::Log
