// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cinttypes>
#include <clocale>
#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>

#include <fmt/ranges.h>

#include "core/hle/service/am/applet_manager.h"
#include "core/loader/nca.h"
#include "core/loader/nro.h"
#include "core/tools/renderdoc.h"

#ifdef __APPLE__
#include <unistd.h> // for chdir
#endif
#ifdef __unix__
#include <csignal>
#include <QtDBus/QDBusInterface>
#include <QtDBus/QDBusMessage>
#include <QtDBus/QtDBus>
#include <sys/socket.h>
#include "common/linux/gamemode.h"
#endif

#include <boost/container/flat_set.hpp>

// VFS includes must be before glad as they will conflict with Windows file api, which uses defines.
#include "applets/qt_amiibo_settings.h"
#include "applets/qt_controller.h"
#include "applets/qt_error.h"
#include "applets/qt_profile_select.h"
#include "applets/qt_software_keyboard.h"
#include "applets/qt_web_browser.h"
#include "common/nvidia_flags.h"
#include "common/settings_enums.h"
#include "configuration/configure_input.h"
#include "configuration/configure_per_game.h"
#include "configuration/configure_tas.h"
#include "core/file_sys/romfs_factory.h"
#include "core/file_sys/vfs/vfs.h"
#include "core/file_sys/vfs/vfs_real.h"
#include "core/frontend/applets/cabinet.h"
#include "core/frontend/applets/controller.h"
#include "core/frontend/applets/general.h"
#include "core/frontend/applets/mii_edit.h"
#include "core/frontend/applets/software_keyboard.h"
#include "core/hle/service/acc/profile_manager.h"
#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/set/system_settings_server.h"
#include "frontend_common/content_manager.h"
#include "hid_core/frontend/emulated_controller.h"
#include "hid_core/hid_core.h"
#include "suyu/multiplayer/state.h"
#include "suyu/util/controller_navigation.h"

// These are wrappers to avoid the calls to CreateDirectory and CreateFile because of the Windows
static FileSys::VirtualDir VfsFilesystemCreateDirectoryWrapper(
    const FileSys::VirtualFilesystem& vfs, const std::string& path, FileSys::OpenMode mode) {
    return vfs->CreateDirectory(path, mode);
}

// Overloaded function, also removed by palafiate
static FileSys::VirtualFile VfsDirectoryCreateFileWrapper(const FileSys::VirtualDir& dir,
                                                          const std::string& path) {
    return dir->CreateFile(path);
}

#include <fmt/ostream.h>
#include <glad/glad.h>

#define QT_NO_OPENGL
#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QStatusBar>
#include <QString>
#include <QSslSocket>
#include <QSysInfo>
#include <QTreeView>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>

#ifdef HAVE_SDL2
#include <SDL.h> // For SDL ScreenSaver functions
#endif

#include <fmt/format.h>
#include "common/detached_tasks.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/literals.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/memory_detect.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#ifdef _WIN32
#include <DbgHelp.h>
#include <shlobj.h>
#include "common/windows/timer_resolution.h"
#pragma comment(lib, "Dbghelp.lib")
#endif
#include "common/cpu_features.h"
#include "common/settings.h"
#include "core/arm/debug.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/crypto/key_manager.h"
#include "core/file_sys/card_image.h"
#include "core/file_sys/common_funcs.h"
#include "core/file_sys/content_archive.h"
#include "core/file_sys/control_metadata.h"
#include "core/file_sys/patch_manager.h"
#include "core/file_sys/registered_cache.h"
#include "core/file_sys/romfs.h"
#include "core/file_sys/savedata_factory.h"
#include "core/file_sys/submission_package.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/filesystem/filesystem.h"
#include "core/hle/service/sm/sm.h"
#include "core/loader/loader.h"
#include "core/perf_stats.h"
#include "frontend_common/config.h"
#include "input_common/drivers/tas_input.h"
#include "input_common/drivers/virtual_amiibo.h"
#include "input_common/main.h"
#include "suyu/about_dialog.h"
#include "suyu/bootmanager.h"
#include "suyu/compatdb.h"
#include "suyu/compatibility_list.h"
#include "suyu/configuration/configure_dialog.h"
#include "suyu/configuration/configure_input_per_game.h"
#include "suyu/configuration/qt_config.h"
#include "suyu/debugger/console.h"
#include "suyu/debugger/controller.h"
#include "suyu/debugger/profiler.h"
#include "suyu/debugger/wait_tree.h"
#include "suyu/discord.h"
#include "suyu/game_list.h"
#include "suyu/game_list_p.h"
#include "suyu/hotkeys.h"
#include "suyu/install_dialog.h"
#include "suyu/loading_screen.h"
#include "suyu/main.h"
#include "suyu/play_time_manager.h"
#include "suyu/startup_checks.h"
#include "suyu/uisettings.h"
#include "suyu/mode_selector.h"
#include "suyu/emulator_core_manager.h"
#include "suyu/hacker_environment.h"
#include "suyu/mcp_server.h"
#include "suyu/programmer_environment.h"
#include "suyu/game_export.h"
#include "suyu/gamer_environment.h"
#include "suyu/nintendo_account.h"
#include "suyu/social_sidebar.h"
#include "suyu/external_decryption_tool.h"
#include "suyu/mods_browser_dialog.h"
#include "suyu/steam_integration.h"
#include "suyu/user_manual_widget.h"
#include "suyu/util/clickable_label.h"
#include "suyu/vk_device_info.h"
#include "ui_main.h"
#include "util/overlay_dialog.h"

static void AppendTerminateMessage(std::string_view message) {
    std::fprintf(stderr, "%.*s\n", static_cast<int>(message.size()), message.data());
    try {
        const auto log_path = Common::FS::GetSuyuPath(Common::FS::SuyuPath::LogDir) /
                              "suyu_log.txt";
        std::ofstream log_file(log_path, std::ios::app);
        if (log_file.is_open()) {
            log_file << "[terminate] " << message << '\n';
        }
    } catch (...) {
    }
}

static void InstallTerminateLogger() {
    std::set_terminate([] {
        std::string message{"Unhandled exception reached std::terminate without details."};
        if (const auto exception = std::current_exception()) {
            try {
                std::rethrow_exception(exception);
            } catch (const std::exception& e) {
                message = fmt::format("Unhandled exception reached std::terminate: {}", e.what());
            } catch (...) {
                message = "Unhandled non-std exception reached std::terminate.";
            }
        }
        AppendTerminateMessage(message);
        std::abort();
    });
}

#ifdef _WIN32
static void LogStackTraceFromContext(CONTEXT context) {
    HANDLE process = GetCurrentProcess();
    HANDLE thread = GetCurrentThread();

    SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
    SymInitialize(process, nullptr, TRUE);

    STACKFRAME64 stack_frame{};
    DWORD machine_type = IMAGE_FILE_MACHINE_AMD64;
    stack_frame.AddrPC.Offset = context.Rip;
    stack_frame.AddrPC.Mode = AddrModeFlat;
    stack_frame.AddrFrame.Offset = context.Rbp;
    stack_frame.AddrFrame.Mode = AddrModeFlat;
    stack_frame.AddrStack.Offset = context.Rsp;
    stack_frame.AddrStack.Mode = AddrModeFlat;

    alignas(SYMBOL_INFO) char symbol_storage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbol_storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    for (int frame_index = 0; frame_index < 32; ++frame_index) {
        if (!StackWalk64(machine_type, process, thread, &stack_frame, &context, nullptr,
                         SymFunctionTableAccess64, SymGetModuleBase64, nullptr) ||
            stack_frame.AddrPC.Offset == 0) {
            break;
        }

        DWORD64 displacement = 0;
        if (SymFromAddr(process, stack_frame.AddrPC.Offset, &displacement, symbol)) {
            AppendTerminateMessage(
                fmt::format("[seh-stack] {} + 0x{:X}", symbol->Name, displacement));
        } else {
            AppendTerminateMessage(fmt::format("[seh-stack] 0x{:X}", stack_frame.AddrPC.Offset));
        }
    }
}

static LONG WINAPI LogUnhandledSehException(EXCEPTION_POINTERS* exception_pointers) {
    const auto exception_code = exception_pointers->ExceptionRecord->ExceptionCode;
    const auto exception_address = exception_pointers->ExceptionRecord->ExceptionAddress;
    AppendTerminateMessage(
        fmt::format("Unhandled SEH exception 0x{:08X} at {}", exception_code, exception_address));

    LogStackTraceFromContext(*exception_pointers->ContextRecord);

    return EXCEPTION_CONTINUE_SEARCH;
}
#endif
#include "video_core/gpu.h"
#include "video_core/renderer_base.h"
#include "video_core/shader_notify.h"

#ifdef SUYU_CRASH_DUMPS
#include "suyu/breakpad.h"
#endif

using namespace Common::Literals;

#ifdef USE_DISCORD_PRESENCE
#include "suyu/discord_impl.h"
#endif

#ifdef QT_STATICPLUGIN
Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin);
#endif

#ifdef _WIN32
#include <windows.h>
extern "C" {
// tells Nvidia and AMD drivers to use the dedicated GPU by default on laptops with switchable
// graphics
__declspec(dllexport) unsigned long NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}
#endif

constexpr int default_mouse_hide_timeout = 2500;
constexpr int default_input_update_timeout = 1;

constexpr size_t CopyBufferSize = 1_MiB;

/**
 * "Callouts" are one-time instructional messages shown to the user. In the config settings, there
 * is a bitfield "callout_flags" options, used to track if a message has already been shown to the
 * user. This is 32-bits - if we have more than 32 callouts, we should retire and recycle old ones.
 */
enum class CalloutFlag : uint32_t {
    DRDDeprecation = 0x2,
};

const int GMainWindow::max_recent_files_item;

static void RemoveCachedContents() {
    const auto cache_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir);
    const auto offline_fonts = cache_dir / "fonts";
    const auto offline_manual = cache_dir / "offline_web_applet_manual";
    const auto offline_legal_information = cache_dir / "offline_web_applet_legal_information";
    const auto offline_system_data = cache_dir / "offline_web_applet_system_data";

    Common::FS::RemoveDirRecursively(offline_fonts);
    Common::FS::RemoveDirRecursively(offline_manual);
    Common::FS::RemoveDirRecursively(offline_legal_information);
    Common::FS::RemoveDirRecursively(offline_system_data);
}

static void LogRuntimes() {
#ifdef _MSC_VER
    // It is possible that the name of the dll will change.
    // vcruntime140.dll is for 2015 and onwards
    static constexpr char runtime_dll_name[] = "vcruntime140.dll";
    UINT sz = GetFileVersionInfoSizeA(runtime_dll_name, nullptr);
    bool runtime_version_inspection_worked = false;
    if (sz > 0) {
        std::vector<u8> buf(sz);
        if (GetFileVersionInfoA(runtime_dll_name, 0, sz, buf.data())) {
            VS_FIXEDFILEINFO* pvi;
            sz = sizeof(VS_FIXEDFILEINFO);
            if (VerQueryValueA(buf.data(), "\\", reinterpret_cast<LPVOID*>(&pvi), &sz)) {
                if (pvi->dwSignature == VS_FFI_SIGNATURE) {
                    runtime_version_inspection_worked = true;
                    LOG_INFO(Frontend, "MSVC Compiler: {} Runtime: {}.{}.{}.{}", _MSC_VER,
                             pvi->dwProductVersionMS >> 16, pvi->dwProductVersionMS & 0xFFFF,
                             pvi->dwProductVersionLS >> 16, pvi->dwProductVersionLS & 0xFFFF);
                }
            }
        }
    }
    if (!runtime_version_inspection_worked) {
        LOG_INFO(Frontend, "Unable to inspect {}", runtime_dll_name);
    }
#endif
    LOG_INFO(Frontend, "Qt Compile: {} Runtime: {}", QT_VERSION_STR, qVersion());
}

static QString PrettyProductName() {
#ifdef _WIN32
    // After Windows 10 Version 2004, Microsoft decided to switch to a different notation: 20H2
    // With that notation change they changed the registry key used to denote the current version
    QSettings windows_registry(
        QStringLiteral("HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion"),
        QSettings::NativeFormat);
    const QString release_id = windows_registry.value(QStringLiteral("ReleaseId")).toString();
    if (release_id == QStringLiteral("2009")) {
        const u32 current_build = windows_registry.value(QStringLiteral("CurrentBuild")).toUInt();
        const QString display_version =
            windows_registry.value(QStringLiteral("DisplayVersion")).toString();
        const u32 ubr = windows_registry.value(QStringLiteral("UBR")).toUInt();
        u32 version = 10;
        if (current_build >= 22000) {
            version = 11;
        }
        return QStringLiteral("Windows %1 Version %2 (Build %3.%4)")
            .arg(QString::number(version), display_version, QString::number(current_build),
                 QString::number(ubr));
    }
#endif
    return QSysInfo::prettyProductName();
}

#ifdef _WIN32
static void OverrideWindowsFont() {
    // Qt5 chooses these fonts on Windows and they have fairly ugly alphanumeric/cyrillic characters
    // Asking to use "MS Shell Dlg 2" gives better other chars while leaving the Chinese Characters.
    const QString startup_font = QApplication::font().family();
    const QStringList ugly_fonts = {QStringLiteral("SimSun"), QStringLiteral("PMingLiU")};
    if (ugly_fonts.contains(startup_font)) {
        QApplication::setFont(QFont(QStringLiteral("MS Shell Dlg 2"), 9, QFont::Normal));
    }
}
#endif

GMainWindow::GMainWindow(std::unique_ptr<QtConfig> config_, bool has_broken_vulkan)
    : ui{std::make_unique<Ui::MainWindow>()}, system{std::make_unique<Core::System>()},
      input_subsystem{std::make_shared<InputCommon::InputSubsystem>()}, config{std::move(config_)},
      vfs{std::make_shared<FileSys::RealVfsFilesystem>()},
      provider{std::make_unique<FileSys::ManualContentProvider>()} {
#ifdef __unix__
    SetupSigInterrupts();
    SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
#endif
    system->Initialize();

    Common::Log::Initialize();
    Common::Log::Start();

    LoadTranslation();

    setAcceptDrops(true);
    ui->setupUi(this);
    statusBar()->hide();

    startup_icon_theme = QIcon::themeName();
    // fallback can only be set once, default theme icons are okay on both light/dark
    QIcon::setFallbackThemeName(QStringLiteral("default"));
    QIcon::setFallbackSearchPaths(QStringList(QStringLiteral(":/icons")));

    default_theme_paths = QIcon::themeSearchPaths();

    SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    discord_rpc->Update();

    play_time_manager = std::make_unique<PlayTime::PlayTimeManager>(system->GetProfileManager());

    system->GetRoomNetwork().Init();

    RegisterMetaTypes();

    InitializeWidgets();
    InitializeDebugWidgets();
    InitializeRecentFileMenuActions();
    InitializeHotkeys();

    SetDefaultUIGeometry();
    RestoreUIState();
    UpdateUITheme();

    ConnectMenuEvents();
    ConnectWidgetEvents();

    system->HIDCore().ReloadInputDevices();
    controller_dialog->refreshConfiguration();

    const auto branch_name = std::string(Common::g_scm_branch);
    const auto description = std::string(Common::g_scm_desc);
    const auto build_id = std::string(Common::g_build_id);

    const auto suyu_build = fmt::format("suyu Development Build | {}-{}", branch_name, description);
    const auto override_build =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto suyu_build_version = override_build.empty() ? suyu_build : override_build;
    const auto processor_count = std::thread::hardware_concurrency();

    LOG_INFO(Frontend, "suyu Version: {}", suyu_build_version);
    LogRuntimes();
#ifdef ARCHITECTURE_x86_64
    const auto& caps = Common::g_cpu_caps;
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512f) {
        cpu_string += " | AVX";
        if (caps.avx512f) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma) {
            cpu_string += " | FMA";
        }
    }
    LOG_INFO(Frontend, "Host CPU: {}", cpu_string);
    if (std::optional<int> processor_core = Common::GetProcessorCount()) {
        LOG_INFO(Frontend, "Host CPU Cores: {}", *processor_core);
    }
#endif
    LOG_INFO(Frontend, "Host CPU Threads: {}", processor_count);
    LOG_INFO(Frontend, "Host OS: {}", PrettyProductName().toStdString());
    LOG_INFO(Frontend, "Host RAM: {:.2f} GiB",
             Common::GetMemInfo().TotalPhysicalMemory / f64{1_GiB});
    LOG_INFO(Frontend, "Host Swap: {:.2f} GiB", Common::GetMemInfo().TotalSwapMemory / f64{1_GiB});
#ifdef _WIN32
    LOG_INFO(Frontend, "Host Timer Resolution: {:.4f} ms",
             std::chrono::duration_cast<std::chrono::duration<f64, std::milli>>(
                 Common::Windows::SetCurrentTimerResolutionToMaximum())
                 .count());
    system->CoreTiming().SetTimerResolutionNs(Common::Windows::GetCurrentTimerResolution());
#endif
    UpdateWindowTitle();

    show();

    system->SetContentProvider(std::make_unique<FileSys::ContentProviderUnion>());
    system->RegisterContentProvider(FileSys::ContentProviderUnionSlot::FrontendManual,
                                    provider.get());
    system->GetFileSystemController().CreateFactories(*vfs);

    // Remove cached contents generated during the previous session
    RemoveCachedContents();

    // Gen keys if necessary
    OnCheckFirmwareDecryption();

    if (UISettings::values.game_dirs.isEmpty() && !UISettings::values.roms_path.empty()) {
        const QString fallback_rom_dir =
            QString::fromStdString(UISettings::values.roms_path);
        if (QDir(fallback_rom_dir).exists()) {
            UISettings::values.game_dirs.append(
                UISettings::GameDir{fallback_rom_dir.toStdString(), true, true});
            LOG_INFO(Frontend, "Auto-added fallback game directory: {}",
                     fallback_rom_dir.toStdString());
        }
    }

    QStringList args = QApplication::arguments();

    auto FindBundledGamePath = [](const QString& exe_path) {
        const QFileInfo exe_info(exe_path);
        const QString base_name = exe_info.completeBaseName();
        if (base_name.isEmpty() ||
            base_name.compare(QStringLiteral("suyu"), Qt::CaseInsensitive) == 0) {
            return QString();
        }

        const QString exe_dir = exe_info.absolutePath();
        const QStringList rom_extensions = {QStringLiteral(".nsp"), QStringLiteral(".xci"),
                                            QStringLiteral(".nca"), QStringLiteral(".nro"),
                                            QStringLiteral(".rom"), QStringLiteral(".bin")};

        for (const QString& ext : rom_extensions) {
            const QString candidate = exe_dir + QDir::separator() + base_name + ext;
            if (QFile::exists(candidate)) {
                return candidate;
            }
        }
        return QString();
    };

    QString game_path;
    bool has_gamepath = false;
    bool is_fullscreen = false;
    bool is_qlaunch = false;

    for (int i = 1; i < args.size(); ++i) {
        // Preserves drag/drop functionality
        if (args.size() == 2 && !args[1].startsWith(QChar::fromLatin1('-'))) {
            game_path = args[1];
            has_gamepath = true;
            break;
        }

        // Launch game in fullscreen mode
        if (args[i] == QStringLiteral("-f")) {
            is_fullscreen = true;
            continue;
        }
        // Use QLaunch at startup
        if (args[i] == QStringLiteral("-ql")) {
            is_qlaunch = true;
            continue;
        }
        // Start in Hacker mode (enables MCP server)
        if (args[i] == QStringLiteral("-hacker")) {
            QTimer::singleShot(0, this, [this]() { ApplyAppMode(AppMode::Hacker); });
            continue;
        }
        // Start in Gamer mode (enables MCP server)
        if (args[i] == QStringLiteral("-gamer")) {
            QTimer::singleShot(0, this, [this]() { ApplyAppMode(AppMode::Gamer); });
            continue;
        }
        // Launch game with a specific user
        if (args[i] == QStringLiteral("-u")) {
            if (i >= args.size() - 1) {
                continue;
            }

            if (args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }

            int user_arg_idx = ++i;
            bool argument_ok;
            std::size_t selected_user = args[user_arg_idx].toUInt(&argument_ok);

            if (!argument_ok) {
                // try to look it up by username, only finds the first username that matches.
                const std::string user_arg_str = args[user_arg_idx].toStdString();
                const auto user_idx = system->GetProfileManager().GetUserIndex(user_arg_str);

                if (user_idx == std::nullopt) {
                    LOG_ERROR(Frontend, "Invalid user argument");
                    continue;
                }

                selected_user = user_idx.value();
            }

            if (!system->GetProfileManager().UserExistsIndex(selected_user)) {
                LOG_ERROR(Frontend, "Selected user doesn't exist");
                continue;
            }

            Settings::values.current_user = static_cast<s32>(selected_user);

            user_flag_cmd_line = true;
            continue;
        }

        // Launch game at path
        if (args[i] == QStringLiteral("-g")) {
            if (i >= args.size() - 1) {
                continue;
            }

            if (args[i + 1].startsWith(QChar::fromLatin1('-'))) {
                continue;
            }

            game_path = args[++i];
            has_gamepath = true;
        }
    }

    // Automatically load a bundled game if this executable is an exported game package.
    if (!has_gamepath) {
        const QString bundled_game_path =
            FindBundledGamePath(QCoreApplication::applicationFilePath());
        if (!bundled_game_path.isEmpty()) {
            game_path = bundled_game_path;
            has_gamepath = true;
        }
    }

    if (!has_gamepath && !is_qlaunch) {
        game_list->LoadCompatibilityList();
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    // make sure menubar has the arrow cursor instead of inheriting from this
    ui->menubar->setCursor(QCursor());
    statusBar()->setCursor(QCursor());

    mouse_hide_timer.setInterval(default_mouse_hide_timeout);
    connect(&mouse_hide_timer, &QTimer::timeout, this, &GMainWindow::HideMouseCursor);
    connect(ui->menubar, &QMenuBar::hovered, this, &GMainWindow::ShowMouseCursor);

    update_input_timer.setInterval(default_input_update_timeout);
    connect(&update_input_timer, &QTimer::timeout, this, &GMainWindow::UpdateInputDrivers);
    update_input_timer.start();

    MigrateConfigFiles();

    if (has_broken_vulkan) {
        UISettings::values.has_broken_vulkan = true;

        QMessageBox::warning(this, tr("Broken Vulkan Installation Detected"),
                             tr("Vulkan initialization failed during boot.<br><br>Click <a "
                                "href='https://suyu-emu.github.io/website/faq'>"
                                "here for instructions to fix the issue</a>."));

#ifdef HAS_OPENGL
        Settings::values.renderer_backend = Settings::RendererBackend::OpenGL_SPIRV;
#else
        Settings::values.renderer_backend = Settings::RendererBackend::Null;
#endif

        UpdateAPIText();
        renderer_status_button->setDisabled(true);
        renderer_status_button->setChecked(false);
    } else {
        VkDeviceInfo::PopulateRecords(vk_device_records, this->window()->windowHandle());
    }

#if defined(HAVE_SDL2) && !defined(_WIN32)
    SDL_InitSubSystem(SDL_INIT_VIDEO);

    // Set a screensaver inhibition reason string. Currently passed to DBus by SDL and visible to
    // the user through their desktop environment.
    //: TRANSLATORS: This string is shown to the user to explain why suyu needs to prevent the
    //: computer from sleeping
    QByteArray wakelock_reason = tr("Running a game").toUtf8();
    SDL_SetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME, wakelock_reason.data());

    // SDL disables the screen saver by default, and setting the hint
    // SDL_HINT_VIDEO_ALLOW_SCREENSAVER doesn't seem to work, so we just enable the screen saver
    // for now.
    SDL_EnableScreenSaver();
#endif

#ifdef __unix__
    SetupPrepareForSleep();
    ListenColorSchemeChange();
#endif

    // Override fullscreen setting if gamepath or argument is provided
    if (has_gamepath || is_fullscreen) {
        ui->action_Fullscreen->setChecked(is_fullscreen);
    }
    // Open HomeMenu
    if (!has_gamepath && is_qlaunch) {
        OnHomeMenu();
    }
    if (!game_path.isEmpty()) {
        BootGame(game_path, ApplicationAppletParameters());
    }
}

GMainWindow::~GMainWindow() {
    // will get automatically deleted otherwise
    if (render_window->parent() == nullptr) {
        delete render_window;
    }

#ifdef __unix__
    ::close(sig_interrupt_fds[0]);
    ::close(sig_interrupt_fds[1]);
#endif
}

void GMainWindow::RegisterMetaTypes() {
    // Register integral and floating point types
    qRegisterMetaType<u8>("u8");
    qRegisterMetaType<u16>("u16");
    qRegisterMetaType<u32>("u32");
    qRegisterMetaType<u64>("u64");
    qRegisterMetaType<u128>("u128");
    qRegisterMetaType<s8>("s8");
    qRegisterMetaType<s16>("s16");
    qRegisterMetaType<s32>("s32");
    qRegisterMetaType<s64>("s64");
    qRegisterMetaType<f32>("f32");
    qRegisterMetaType<f64>("f64");

    // Register string types
    qRegisterMetaType<std::string>("std::string");
    qRegisterMetaType<std::wstring>("std::wstring");
    qRegisterMetaType<std::u8string>("std::u8string");
    qRegisterMetaType<std::u16string>("std::u16string");
    qRegisterMetaType<std::u32string>("std::u32string");
    qRegisterMetaType<std::string_view>("std::string_view");
    qRegisterMetaType<std::wstring_view>("std::wstring_view");
    qRegisterMetaType<std::u8string_view>("std::u8string_view");
    qRegisterMetaType<std::u16string_view>("std::u16string_view");
    qRegisterMetaType<std::u32string_view>("std::u32string_view");

    // Register applet types

    // Cabinet Applet
    qRegisterMetaType<Core::Frontend::CabinetParameters>("Core::Frontend::CabinetParameters");
    qRegisterMetaType<std::shared_ptr<Service::NFC::NfcDevice>>(
        "std::shared_ptr<Service::NFC::NfcDevice>");

    // Controller Applet
    qRegisterMetaType<Core::Frontend::ControllerParameters>("Core::Frontend::ControllerParameters");

    // Profile Select Applet
    qRegisterMetaType<Core::Frontend::ProfileSelectParameters>(
        "Core::Frontend::ProfileSelectParameters");

    // Software Keyboard Applet
    qRegisterMetaType<Core::Frontend::KeyboardInitializeParameters>(
        "Core::Frontend::KeyboardInitializeParameters");
    qRegisterMetaType<Core::Frontend::InlineAppearParameters>(
        "Core::Frontend::InlineAppearParameters");
    qRegisterMetaType<Core::Frontend::InlineTextParameters>("Core::Frontend::InlineTextParameters");
    qRegisterMetaType<Service::AM::Frontend::SwkbdResult>("Service::AM::Frontend::SwkbdResult");
    qRegisterMetaType<Service::AM::Frontend::SwkbdTextCheckResult>(
        "Service::AM::Frontend::SwkbdTextCheckResult");
    qRegisterMetaType<Service::AM::Frontend::SwkbdReplyType>(
        "Service::AM::Frontend::SwkbdReplyType");

    // Web Browser Applet
    qRegisterMetaType<Service::AM::Frontend::WebExitReason>("Service::AM::Frontend::WebExitReason");

    // Register loader types
    qRegisterMetaType<Core::SystemResultStatus>("Core::SystemResultStatus");
}

void GMainWindow::AmiiboSettingsShowDialog(const Core::Frontend::CabinetParameters& parameters,
                                           std::shared_ptr<Service::NFC::NfcDevice> nfp_device) {
    cabinet_applet =
        new QtAmiiboSettingsDialog(this, parameters, input_subsystem.get(), nfp_device);
    SCOPE_EXIT {
        cabinet_applet->deleteLater();
        cabinet_applet = nullptr;
    };

    cabinet_applet->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint |
                                   Qt::WindowTitleHint | Qt::WindowSystemMenuHint);
    cabinet_applet->setWindowModality(Qt::WindowModal);

    if (cabinet_applet->exec() == QDialog::Rejected) {
        emit AmiiboSettingsFinished(false, {});
        return;
    }

    emit AmiiboSettingsFinished(true, cabinet_applet->GetName());
}

void GMainWindow::AmiiboSettingsRequestExit() {
    if (cabinet_applet) {
        cabinet_applet->reject();
    }
}

void GMainWindow::ControllerSelectorReconfigureControllers(
    const Core::Frontend::ControllerParameters& parameters) {
    controller_applet =
        new QtControllerSelectorDialog(this, parameters, input_subsystem.get(), *system);
    SCOPE_EXIT {
        controller_applet->deleteLater();
        controller_applet = nullptr;
    };

    controller_applet->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
                                      Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint);
    controller_applet->setWindowModality(Qt::WindowModal);
    bool is_success = controller_applet->exec() != QDialog::Rejected;

    // Don't forget to apply settings.
    system->HIDCore().DisableAllControllerConfiguration();
    system->ApplySettings();
    config->SaveAllValues();

    UpdateStatusButtons();

    emit ControllerSelectorReconfigureFinished(is_success);
}

void GMainWindow::ControllerSelectorRequestExit() {
    if (controller_applet) {
        controller_applet->reject();
    }
}

void GMainWindow::ProfileSelectorSelectProfile(
    const Core::Frontend::ProfileSelectParameters& parameters) {
    profile_select_applet = new QtProfileSelectionDialog(*system, this, parameters);
    SCOPE_EXIT {
        profile_select_applet->deleteLater();
        profile_select_applet = nullptr;
    };

    profile_select_applet->setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint |
                                          Qt::WindowStaysOnTopHint | Qt::WindowTitleHint |
                                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    profile_select_applet->setWindowModality(Qt::WindowModal);
    if (profile_select_applet->exec() == QDialog::Rejected) {
        emit ProfileSelectorFinishedSelection(std::nullopt);
        return;
    }

    const auto uuid = system->GetProfileManager().GetUser(
        static_cast<std::size_t>(profile_select_applet->GetIndex()));
    if (!uuid.has_value()) {
        emit ProfileSelectorFinishedSelection(std::nullopt);
        return;
    }

    emit ProfileSelectorFinishedSelection(uuid);
}

void GMainWindow::ProfileSelectorRequestExit() {
    if (profile_select_applet) {
        profile_select_applet->reject();
    }
}

void GMainWindow::SoftwareKeyboardInitialize(
    bool is_inline, Core::Frontend::KeyboardInitializeParameters initialize_parameters) {
    if (software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is already initialized!");
        return;
    }

    software_keyboard = new QtSoftwareKeyboardDialog(render_window, *system, is_inline,
                                                     std::move(initialize_parameters));

    if (is_inline) {
        connect(
            software_keyboard, &QtSoftwareKeyboardDialog::SubmitInlineText, this,
            [this](Service::AM::Frontend::SwkbdReplyType reply_type, std::u16string submitted_text,
                   s32 cursor_position) {
                emit SoftwareKeyboardSubmitInlineText(reply_type, submitted_text, cursor_position);
            },
            Qt::QueuedConnection);
    } else {
        connect(
            software_keyboard, &QtSoftwareKeyboardDialog::SubmitNormalText, this,
            [this](Service::AM::Frontend::SwkbdResult result, std::u16string submitted_text,
                   bool confirmed) {
                emit SoftwareKeyboardSubmitNormalText(result, submitted_text, confirmed);
            },
            Qt::QueuedConnection);
    }
}

void GMainWindow::SoftwareKeyboardShowNormal() {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    const auto& layout = render_window->GetFramebufferLayout();

    const auto x = layout.screen.left;
    const auto y = layout.screen.top;
    const auto w = layout.screen.GetWidth();
    const auto h = layout.screen.GetHeight();
    const auto scale_ratio = devicePixelRatioF();

    software_keyboard->ShowNormalKeyboard(render_window->mapToGlobal(QPoint(x, y) / scale_ratio),
                                          QSize(w, h) / scale_ratio);
}

void GMainWindow::SoftwareKeyboardShowTextCheck(
    Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
    std::u16string text_check_message) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->ShowTextCheckDialog(text_check_result, text_check_message);
}

void GMainWindow::SoftwareKeyboardShowInline(
    Core::Frontend::InlineAppearParameters appear_parameters) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    const auto& layout = render_window->GetFramebufferLayout();

    const auto x =
        static_cast<int>(layout.screen.left + (0.5f * layout.screen.GetWidth() *
                                               ((2.0f * appear_parameters.key_top_translate_x) +
                                                (1.0f - appear_parameters.key_top_scale_x))));
    const auto y =
        static_cast<int>(layout.screen.top + (layout.screen.GetHeight() *
                                              ((2.0f * appear_parameters.key_top_translate_y) +
                                               (1.0f - appear_parameters.key_top_scale_y))));
    const auto w = static_cast<int>(layout.screen.GetWidth() * appear_parameters.key_top_scale_x);
    const auto h = static_cast<int>(layout.screen.GetHeight() * appear_parameters.key_top_scale_y);
    const auto scale_ratio = devicePixelRatioF();

    software_keyboard->ShowInlineKeyboard(std::move(appear_parameters),
                                          render_window->mapToGlobal(QPoint(x, y) / scale_ratio),
                                          QSize(w, h) / scale_ratio);
}

void GMainWindow::SoftwareKeyboardHideInline() {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->HideInlineKeyboard();
}

void GMainWindow::SoftwareKeyboardInlineTextChanged(
    Core::Frontend::InlineTextParameters text_parameters) {
    if (!software_keyboard) {
        LOG_ERROR(Frontend, "The software keyboard is not initialized!");
        return;
    }

    software_keyboard->InlineTextChanged(std::move(text_parameters));
}

void GMainWindow::SoftwareKeyboardExit() {
    if (!software_keyboard) {
        return;
    }

    software_keyboard->ExitKeyboard();

    software_keyboard = nullptr;
}

void GMainWindow::WebBrowserOpenWebPage(const std::string& main_url,
                                        const std::string& additional_args, bool is_local) {
#ifdef SUYU_USE_QT_WEB_ENGINE

    // Raw input breaks with the web applet, Disable web applets if enabled
    if (UISettings::values.disable_web_applet || Settings::values.enable_raw_input) {
        emit WebBrowserClosed(Service::AM::Frontend::WebExitReason::WindowClosed,
                              "http://localhost/");
        return;
    }

    web_applet = new QtNXWebEngineView(this, *system, input_subsystem.get());

    ui->action_Pause->setEnabled(false);
    ui->action_Restart->setEnabled(false);
    ui->action_Stop->setEnabled(false);

    {
        QProgressDialog loading_progress(this);
        loading_progress.setLabelText(tr("Loading Web Applet..."));
        loading_progress.setRange(0, 3);
        loading_progress.setValue(0);

        if (is_local && !Common::FS::Exists(main_url)) {
            loading_progress.show();

            auto future = QtConcurrent::run([this] { emit WebBrowserExtractOfflineRomFS(); });

            while (!future.isFinished()) {
                QCoreApplication::processEvents();

                std::this_thread::itional_args);
        }

        if (render_window->IsLoadingComplete()) {
            render_window->hide();
        }

        const auto& layout = render_window->GetFramebufferLayout();
        const auto scale_ratio = devicePixelRatioF();
        web_applet->resize(layout.screen.GetWidth() / scale_ratio,
                           layout.screen.GetHeight() / scale_ratio);
        web_applet->move(layout.screen.left / scale_ratio,
                         (layout.screen.top / scale_ratio) + menuBar()->height());
        web_applet->setZoomFactor(static_cast<qreal>(layout.screen.GetWidth() / scale_ratio) /
                                  static_cast<qreal>(Layout::ScreenUndocked::Width));

        web_applet->setFocus();
        web_applet->show();

        loading_progress.setValue(2);

        QCoreApplication::processEvents();

        loading_progress.setValue(3);
    }

    bool exit_check = false;

    // TODO (Morph): Remove this
    QAction* exit_action = new QAction(tr("Disable Web Applet"), this);
    connect(exit_action, &QAction::triggered, this, [this] {
        const auto result = QMessageBox::warning(
            this, tr("Disable Web Applet"),
            tr("Disabling the web applet can lead to undefined behavior and should only be used "
               "with Super Mario 3D All-Stars. Are you sure you want to disable the web "
               "applet?\n(This can be re-enabled in the Debug settings.)"),
            QMessageBox::Yes | QMessageBox::No);
        if (result == QMessageBox::Yes) {
            UISettings::values.disable_web_applet = true;
            web_applet->SetFinished(true);
        }
    });
    ui->menubar->addAction(exit_action);

    while (!web_applet->IsFinished()) {
        QCoreApplication::processEvents();

        if (!exit_check) {
            web_applet->page()->runJavaScript(
                QStringLiteral("end_applet;"), [&](const QVariant& variant) {
                    exit_check = false;
                    if (variant.toBool()) {
                        web_applet->SetFinished(true);
                        web_applet->SetExitReason(
                            Service::AM::Frontend::WebExitReason::EndButtonPressed);
                    }
                });

            exit_check = true;
        }

        if (web_applet->GetCurrentURL().contains(QStringLiteral("localhost"))) {
            if (!web_applet->IsFinished()) {
                web_applet->SetFinished(true);
                web_applet->SetExitReason(Service::AM::Frontend::WebExitReason::CallbackURL);
            }

            web_applet->SetLastURL(web_applet->GetCurrentURL().toStdString());
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const auto exit_reason = web_applet->GetExitReason();
    const auto last_url = web_applet->GetLastURL();

    web_applet->hide();

    render_window->setFocus();

    if (render_window->IsLoadingComplete()) {
        render_window->show();
    }

    ui->action_Pause->setEnabled(true);
    ui->action_Restart->setEnabled(true);
    ui->action_Stop->setEnabled(true);

    ui->menubar->removeAction(exit_action);

    QCoreApplication::processEvents();

    emit WebBrowserClosed(exit_reason, last_url);

#else

    // Utilize the same fallback as the default web browser applet.
    emit WebBrowserClosed(Service::AM::Frontend::WebExitReason::WindowClosed, "http://localhost/");

#endif
}

void GMainWindow::WebBrowserRequestExit() {
#ifdef SUYU_USE_QT_WEB_ENGINE
    if (web_applet) {
        web_applet->SetExitReason(Service::AM::Frontend::WebExitReason::ExitRequested);
        web_applet->SetFinished(true);
    }
#endif
}

void GMainWindow::InitializeWidgets() {
#ifdef SUYU_ENABLE_COMPATIBILITY_REPORTING
    ui->action_Report_Compatibility->setVisible(true);
#endif
    render_window = new GRenderWindow(this, emu_thread.get(), input_subsystem, *system);
    render_window->hide();

    game_list = new GameList(vfs, provider.get(), *play_time_manager, *system, this);
    ui->emulationLayout->addWidget(game_list);
        // Switch to emulationPage — all content (game_list, programmer_env_, render_window) lives here
        ui->centralStack->setCurrentIndex(1);

    game_list_placeholder = new GameListPlaceholder(this);
    ui->emulationLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    loading_screen = new LoadingScreen(ui->emulationPage);
    loading_screen->hide();
    connect(loading_screen, &LoadingScreen::Hidden, [&] {
        loading_screen->Clear();
        if (emulation_running) {
            render_window->show();
            render_window->setFocus();
        }
    });

    multiplayer_state = new MultiplayerState(this, game_list->GetModel(), ui->action_Leave_Room,
                                             ui->action_Show_Room, *system);
    multiplayer_state->setVisible(false);

    // Create status bar
    message_label = new QLabel();
    // Configured separately for left alignment
    message_label->setFrameStyle(QFrame::NoFrame);
    message_label->setContentsMargins(4, 0, 4, 0);
    message_label->setAlignment(Qt::AlignLeft);
    statusBar()->addPermanentWidget(message_label, 1);

    shader_building_label = new QLabel();
    shader_building_label->setToolTip(tr("The amount of shaders currently being built"));
    res_scale_label = new QLabel();
    res_scale_label->setToolTip(tr("The current selected resolution scaling multiplier."));
    emu_speed_label = new QLabel();
    emu_speed_label->setToolTip(
        tr("Current emulation speed. Values higher or lower than 100% "
           "indicate emulation is running faster or slower than a Switch."));
    game_fps_label = new QLabel();
    game_fps_label->setToolTip(tr("How many frames per second the game is currently displaying. "
                                  "This will vary from game to game and scene to scene."));
    emu_frametime_label = new QLabel();
    emu_frametime_label->setToolTip(
        tr("Time taken to emulate a Switch frame, not counting framelimiting or v-sync. For "
           "full-speed emulation this should be at most 16.67 ms."));

    for (auto& label : {shader_building_label, res_scale_label, emu_speed_label, game_fps_label,
                        emu_frametime_label}) {
        label->setVisible(false);
        label->setFrameStyle(QFrame::NoFrame);
        label->setContentsMargins(4, 0, 4, 0);
        statusBar()->addPermanentWidget(label);
    }

    firmware_label = new QLabel();
    firmware_label->setObjectName(QStringLiteral("FirmwareLabel"));
    firmware_label->setVisible(false);
    firmware_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->addPermanentWidget(firmware_label);

    qt_ssl_available_ = QSslSocket::supportsSsl();
    qt_ssl_build_version_ = QSslSocket::sslLibraryBuildVersionString();
    qt_ssl_runtime_version_ = QSslSocket::sslLibraryVersionString();

    ssl_status_label = new QLabel();
    ssl_status_label->setObjectName(QStringLiteral("SslStatusLabel"));
    ssl_status_label->setFocusPolicy(Qt::NoFocus);
    ssl_status_label->setText(qt_ssl_available_ ? tr("SSL OK") : tr("SSL Missing"));
    ssl_status_label->setToolTip(
        tr("Qt SSL startup check\nBuild SSL: %1\nRuntime SSL: %2")
            .arg(qt_ssl_build_version_.isEmpty() ? tr("unknown") : qt_ssl_build_version_,
                 qt_ssl_runtime_version_.isEmpty() ? tr("unavailable") : qt_ssl_runtime_version_));
    statusBar()->addPermanentWidget(ssl_status_label);
    if (!qt_ssl_available_) {
        LOG_WARNING(Frontend, "Qt SSL support missing at startup (build='{}', runtime='{}')",
                    qt_ssl_build_version_.toStdString(), qt_ssl_runtime_version_.toStdString());
        statusBar()->showMessage(
            tr("SSL runtime libraries are missing; HTTPS features (social/web APIs) may fail."),
            12000);
    }

    statusBar()->addPermanentWidget(multiplayer_state->GetStatusText(), 0);
    statusBar()->addPermanentWidget(multiplayer_state->GetStatusIcon(), 0);

    tas_label = new QLabel();
    tas_label->setObjectName(QStringLiteral("TASlabel"));
    tas_label->setFocusPolicy(Qt::NoFocus);
    statusBar()->insertPermanentWidget(0, tas_label);

    volume_popup = new QWidget(this);
    volume_popup->setWindowFlags(Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint | Qt::Popup);
    volume_popup->setLayout(new QVBoxLayout());
    volume_popup->setMinimumWidth(200);

    volume_slider = new QSlider(Qt::Horizontal);
    volume_slider->setObjectName(QStringLiteral("volume_slider"));
    volume_slider->setMaximum(200);
    volume_slider->setPageStep(5);
    volume_popup->layout()->addWidget(volume_slider);

    volume_button = new VolumeButton();
    volume_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    volume_button->setFocusPolicy(Qt::NoFocus);
    volume_button->setCheckable(true);
    UpdateVolumeUI();
    connect(volume_slider, &QSlider::valueChanged, this, [this](int percentage) {
        Settings::values.audio_muted = false;
        const auto volume = static_cast<u8>(percentage);
        Settings::values.volume.SetValue(volume);
        UpdateVolumeUI();
    });
    connect(volume_button, &QPushButton::clicked, this, [&] {
        UpdateVolumeUI();
        volume_popup->setVisible(!volume_popup->isVisible());
        QRect rect = volume_button->geometry();
        QPoint bottomLeft = statusBar()->mapToGlobal(rect.topLeft());
        bottomLeft.setY(bottomLeft.y() - volume_popup->geometry().height());
        volume_popup->setGeometry(QRect(bottomLeft, QSize(rect.width(), rect.height())));
    });
    volume_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(volume_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                context_menu.addAction(
                    Settings::values.audio_muted ? tr("Unmute") : tr("Mute"), [this] {
                        Settings::values.audio_muted = !Settings::values.audio_muted;
                        UpdateVolumeUI();
                    });

                context_menu.addAction(tr("Reset Volume"), [this] {
                    Settings::values.volume.SetValue(100);
                    UpdateVolumeUI();
                });

                context_menu.exec(volume_button->mapToGlobal(menu_location));
                volume_button->repaint();
            });
    connect(volume_button, &VolumeButton::VolumeChanged, this, &GMainWindow::UpdateVolumeUI);

    statusBar()->insertPermanentWidget(0, volume_button);

    // setup AA button
    aa_status_button = new QPushButton();
    aa_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    aa_status_button->setFocusPolicy(Qt::NoFocus);
    connect(aa_status_button, &QPushButton::clicked, [&] {
        auto aa_mode = Settings::values.anti_aliasing.GetValue();
        aa_mode = static_cast<Settings::AntiAliasing>(static_cast<u32>(aa_mode) + 1);
        if (static_cast<u32>(aa_mode) > static_cast<u32>(Settings::EnumMetadata<Settings::AntiAliasing>::GetLast())) {
            aa_mode = Settings::AntiAliasing::None;
        }
        Settings::values.anti_aliasing.SetValue(aa_mode);
        aa_status_button->setChecked(true);
        UpdateAAText();
    });
    UpdateAAText();
    aa_status_button->setCheckable(true);
    aa_status_button->setChecked(true);
    aa_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(aa_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                for (auto const& aa_text_pair : ConfigurationShared::anti_aliasing_texts_map) {
                    context_menu.addAction(aa_text_pair.second, [this, aa_text_pair] {
                        Settings::values.anti_aliasing.SetValue(aa_text_pair.first);
                        UpdateAAText();
                    });
                }
                context_menu.exec(aa_status_button->mapToGlobal(menu_location));
                aa_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, aa_status_button);

    // Setup Filter button
    filter_status_button = new QPushButton();
    filter_status_button->setObjectName(QStringLiteral("TogglableStatusBarButton"));
    filter_status_button->setFocusPolicy(Qt::NoFocus);
    connect(filter_status_button, &QPushButton::clicked, this,
            &GMainWindow::OnToggleAdaptingFilter);
    UpdateFilterText();
    filter_status_button->setCheckable(true);
    filter_status_button->setChecked(true);
    filter_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(filter_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;
                for (auto const& filter_text_pair : ConfigurationShared::scaling_filter_texts_map) {
                    context_menu.addAction(filter_text_pair.second, [this, filter_text_pair] {
                        Settings::values.scaling_filter.SetValue(filter_text_pair.first);
                        UpdateFilterText();
                    });
                }
                context_menu.exec(filter_status_button->mapToGlobal(menu_location));
                filter_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, filter_status_button);

    // Setup Dock button
    dock_status_button = new QPushButton();
    dock_status_button->setObjectName(QStringLiteral("DockingStatusBarButton"));
    dock_status_button->setFocusPolicy(Qt::NoFocus);
    connect(dock_status_button, &QPushButton::clicked, this, &GMainWindow::OnToggleDockedMode);
    dock_status_button->setCheckable(true);
    UpdateDockedButton();
    dock_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(dock_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;

                for (auto const& pair : ConfigurationShared::use_docked_mode_texts_map) {
                    context_menu.addAction(pair.second, [this, &pair] {
                        if (pair.first != Settings::values.use_docked_mode.GetValue()) {
                            OnToggleDockedMode();
                        }
                    });
                }
                context_menu.exec(dock_status_button->mapToGlobal(menu_location));
                dock_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, dock_status_button);

    // Setup GPU Accuracy button
    gpu_accuracy_button = new QPushButton();
    gpu_accuracy_button->setObjectName(QStringLiteral("GPUStatusBarButton"));
    gpu_accuracy_button->setCheckable(true);
    gpu_accuracy_button->setFocusPolicy(Qt::NoFocus);
    connect(gpu_accuracy_button, &QPushButton::clicked, this, &GMainWindow::OnToggleGpuAccuracy);
    UpdateGPUAccuracyButton();
    gpu_accuracy_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(gpu_accuracy_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;

                for (auto const& gpu_accuracy_pair : ConfigurationShared::gpu_accuracy_texts_map) {
                    context_menu.addAction(gpu_accuracy_pair.second, [this, gpu_accuracy_pair] {
                        Settings::values.gpu_accuracy.SetValue(gpu_accuracy_pair.first);
                        UpdateGPUAccuracyButton();
                    });
                }
                context_menu.exec(gpu_accuracy_button->mapToGlobal(menu_location));
                gpu_accuracy_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, gpu_accuracy_button);

    // Setup Renderer API button
    renderer_status_button = new QPushButton();
    renderer_status_button->setObjectName(QStringLiteral("RendererStatusBarButton"));
    renderer_status_button->setCheckable(true);
    renderer_status_button->setFocusPolicy(Qt::NoFocus);
    connect(renderer_status_button, &QPushButton::clicked, this, &GMainWindow::OnToggleGraphicsAPI);
    UpdateAPIText();
    renderer_status_button->setCheckable(true);
    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
    renderer_status_button->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(renderer_status_button, &QPushButton::customContextMenuRequested,
            [this](const QPoint& menu_location) {
                QMenu context_menu;

                for (auto const& renderer_backend_pair :
                     ConfigurationShared::renderer_backend_texts_map) {
                    if (renderer_backend_pair.first == Settings::RendererBackend::Null) {
                        continue;
                    }
                    context_menu.addAction(
                        renderer_backend_pair.second, [this, renderer_backend_pair] {
                            Settings::values.renderer_backend.SetValue(renderer_backend_pair.first);
                            UpdateAPIText();
                        });
                }
                context_menu.exec(renderer_status_button->mapToGlobal(menu_location));
                renderer_status_button->repaint();
            });
    statusBar()->insertPermanentWidget(0, renderer_status_button);

    statusBar()->setVisible(true);
    setStyleSheet(QStringLiteral("QStatusBar::item{border: none;}"));
}

void GMainWindow::InitializeDebugWidgets() {
    QMenu* debug_menu = ui->menu_View_Debugging;

#if MICROPROFILE_ENABLED
    microProfileDialog = new MicroProfileDialog(this);
    microProfileDialog->hide();
    debug_menu->addAction(microProfileDialog->toggleViewAction());
#endif

    waitTreeWidget = new WaitTreeWidget(*system, this);
    addDockWidget(Qt::LeftDockWidgetArea, waitTreeWidget);
    waitTreeWidget->hide();
    debug_menu->addAction(waitTreeWidget->toggleViewAction());

    controller_dialog = new ControllerDialog(system->HIDCore(), input_subsystem, this);
    controller_dialog->hide();
    debug_menu->addAction(controller_dialog->toggleViewAction());

    connect(this, &GMainWindow::EmulationStarting, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, waitTreeWidget,
            &WaitTreeWidget::OnEmulationStopping);
}

void GMainWindow::InitializeRecentFileMenuActions() {
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], &QAction::triggered, this, &GMainWindow::OnMenuRecentFile);

        ui->menu_recent_files->addAction(actions_recent_files[i]);
    }
    ui->menu_recent_files->addSeparator();
    QAction* action_clear_recent_files = new QAction(this);
    action_clear_recent_files->setText(tr("&Clear Recent Files"));
    connect(action_clear_recent_files, &QAction::triggered, this, [this] {
        UISettings::values.recent_files.clear();
        UpdateRecentFiles();
    });
    ui->menu_recent_files->addAction(action_clear_recent_files);

    UpdateRecentFiles();
}

void GMainWindow::LinkActionShortcut(QAction* action, const QString& action_name,
                                     const bool tas_allowed) {
    static const auto main_window = std::string("Main Window");
    action->setShortcut(hotkey_registry.GetKeySequence(main_window, action_name.toStdString()));
    action->setShortcutContext(
        hotkey_registry.GetShortcutContext(main_window, action_name.toStdString()));
    action->setAutoRepeat(false);

    this->addAction(action);

    auto* controller = system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    const auto* controller_hotkey =
        hotkey_registry.GetControllerHotkey(main_window, action_name.toStdString(), controller);
    connect(
        controller_hotkey, &ControllerShortcut::Activated, this,
        [action, tas_allowed, this] {
            auto [tas_status, current_tas_frame, total_tas_frames] =
                input_subsystem->GetTas()->GetStatus();
            if (tas_allowed || tas_status == InputCommon::TasInput::TasState::Stopped) {
                action->trigger();
            }
        },
        Qt::QueuedConnection);
}

void GMainWindow::InitializeHotkeys() {
    hotkey_registry.LoadHotkeys();

    LinkActionShortcut(ui->action_Load_File, QStringLiteral("Load File"));
    LinkActionShortcut(ui->action_Load_Amiibo, QStringLiteral("Load/Remove Amiibo"));
    LinkActionShortcut(ui->action_Exit, QStringLiteral("Exit suyu"));
    LinkActionShortcut(ui->action_Restart, QStringLiteral("Restart Emulation"));
    LinkActionShortcut(ui->action_Pause, QStringLiteral("Continue/Pause Emulation"));
    LinkActionShortcut(ui->action_Stop, QStringLiteral("Stop Emulation"));
    LinkActionShortcut(ui->action_Show_Filter_Bar, QStringLiteral("Toggle Filter Bar"));
    LinkActionShortcut(ui->action_Show_Status_Bar, QStringLiteral("Toggle Status Bar"));
    LinkActionShortcut(ui->action_Fullscreen, QStringLiteral("Fullscreen"));
    LinkActionShortcut(ui->action_Capture_Screenshot, QStringLiteral("Capture Screenshot"));
    LinkActionShortcut(ui->action_TAS_Start, QStringLiteral("TAS Start/Stop"), true);
    LinkActionShortcut(ui->action_TAS_Record, QStringLiteral("TAS Record"), true);
    LinkActionShortcut(ui->action_TAS_Reset, QStringLiteral("TAS Reset"), true);
    LinkActionShortcut(ui->action_View_Lobby,
                       QStringLiteral("Multiplayer Browse Public Game Lobby"));
    LinkActionShortcut(ui->action_Start_Room, QStringLiteral("Multiplayer Create Room"));
    LinkActionShortcut(ui->action_Connect_To_Room,
                       QStringLiteral("Multiplayer Direct Connect to Room"));
    LinkActionShortcut(ui->action_Show_Room, QStringLiteral("Multiplayer Show Current Room"));
    LinkActionShortcut(ui->action_Leave_Room, QStringLiteral("Multiplayer Leave Room"));

    static const QString main_window = QStringLiteral("Main Window");
    const auto connect_shortcut = [&]<typename Fn>(const QString& action_name, const Fn& function) {
        const auto* hotkey =
            hotkey_registry.GetHotkey(main_window.toStdString(), action_name.toStdString(), this);
        auto* controller = system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
        const auto* controller_hotkey = hotkey_registry.GetControllerHotkey(
            main_window.toStdString(), action_name.toStdString(), controller);
        connect(hotkey, &QShortcut::activated, this, function);
        connect(controller_hotkey, &ControllerShortcut::Activated, this, function,
                Qt::QueuedConnection);
    };

    connect_shortcut(QStringLiteral("Exit Fullscreen"), [&] {
        if (emulation_running && ui->action_Fullscreen->isChecked()) {
            ui->action_Fullscreen->setChecked(false);
            ToggleFullscreen();
        }
    });
    connect_shortcut(QStringLiteral("Change Adapting Filter"),
                     &GMainWindow::OnToggleAdaptingFilter);
    connect_shortcut(QStringLiteral("Change Docked Mode"), &GMainWindow::OnToggleDockedMode);
    connect_shortcut(QStringLiteral("Change GPU Accuracy"), &GMainWindow::OnToggleGpuAccuracy);
    connect_shortcut(QStringLiteral("Audio Mute/Unmute"), &GMainWindow::OnMute);
    connect_shortcut(QStringLiteral("Audio Volume Down"), &GMainWindow::OnDecreaseVolume);
    connect_shortcut(QStringLiteral("Audio Volume Up"), &GMainWindow::OnIncreaseVolume);
    connect_shortcut(QStringLiteral("Toggle Framerate Limit"), [] {
        Settings::values.use_speed_limit.SetValue(!Settings::values.use_speed_limit.GetValue());
    });
    connect_shortcut(QStringLiteral("Toggle Renderdoc Capture"), [this] {
        if (Settings::values.enable_renderdoc_hotkey) {
            system->GetRenderdocAPI().ToggleCapture();
        }
    });
    connect_shortcut(QStringLiteral("Toggle Mouse Panning"), [&] {
        Settings::values.mouse_panning = !Settings::values.mouse_panning;
        if (Settings::values.mouse_panning) {
            render_window->installEventFilter(render_window);
            render_window->setAttribute(Qt::WA_Hover, true);
        }
    });
}

void GMainWindow::SetDefaultUIGeometry() {
    // geometry: 53% of the window contents are in the upper screen half, 47% in the lower half
    const QRect screenRect = QGuiApplication::primaryScreen()->geometry();

    const int w = screenRect.width() * 2 / 3;
    const int h = screenRect.height() * 2 / 3;
    const int x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    const int y = (screenRect.y() + screenRect.height()) / 2 - h * 53 / 100;

    setGeometry(x, y, w, h);
}

void GMainWindow::RestoreUIState() {
    setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
    restoreGeometry(UISettings::values.geometry);
    // Work-around because the games list isn't supposed to be full screen
    if (isFullScreen()) {
        showNormal();
    }
    restoreState(UISettings::values.state);
    render_window->setWindowFlags(render_window->windowFlags() & ~Qt::FramelessWindowHint);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
#if MICROPROFILE_ENABLED
    microProfileDialog->restoreGeometry(UISettings::values.microprofile_geometry);
    microProfileDialog->setVisible(UISettings::values.microprofile_visible.GetValue());
#endif

    game_list->LoadInterfaceLayout();

    ui->action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode.GetValue());
    ToggleWindowMode();

    ui->action_Fullscreen->setChecked(UISettings::values.fullscreen.GetValue());

    ui->action_Display_Dock_Widget_Headers->setChecked(
        UISettings::values.display_titlebar.GetValue());
    OnDisplayTitleBars(ui->action_Display_Dock_Widget_Headers->isChecked());

    ui->action_Show_Filter_Bar->setChecked(UISettings::values.show_filter_bar.GetValue());
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());

    ui->action_Show_Status_Bar->setChecked(UISettings::values.show_status_bar.GetValue());
    ui->action_Show_Folders_In_List->setChecked(UISettings::values.show_folders_in_list.GetValue());
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
    DebuggerSuyu::ToggleConsole();
}

void GMainWindow::OnAppFocusStateChanged(Qt::ApplicationState state) {
    if (state != Qt::ApplicationHidden && state != Qt::ApplicationInactive &&
        state != Qt::ApplicationActive) {
        LOG_DEBUG(Frontend, "ApplicationState unusual flag: {} ", state);
    }
    if (!emulation_running) {
        return;
    }
    if (UISettings::values.pause_when_in_background) {
        if (emu_thread->IsRunning() &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            auto_paused = true;
            OnPauseGame();
        } else if (!emu_thread->IsRunning() && auto_paused && state == Qt::ApplicationActive) {
            auto_paused = false;
            OnStartGame();
        }
    }
    if (UISettings::values.mute_when_in_background) {
        if (!Settings::values.audio_muted &&
            (state & (Qt::ApplicationHidden | Qt::ApplicationInactive))) {
            Settings::values.audio_muted = true;
            auto_muted = true;
        } else if (auto_muted && state == Qt::ApplicationActive) {
            Settings::values.audio_muted = false;
            auto_muted = false;
        }
        UpdateVolumeUI();
    }
}

void GMainWindow::ConnectWidgetEvents() {
    connect(game_list, &GameList::BootGame, this, &GMainWindow::BootGameFromList);
    connect(game_list, &GameList::GameChosen, this, &GMainWindow::OnGameListLoadFile);
    connect(game_list, &GameList::OpenDirectory, this, &GMainWindow::OnGameListOpenDirectory);
    connect(game_list, &GameList::OpenFolderRequested, this, &GMainWindow::OnGameListOpenFolder);
    connect(game_list, &GameList::OpenTransferableShaderCacheRequested, this,
            &GMainWindow::OnTransferableShaderCacheOpenFile);
    connect(game_list, &GameList::RemoveInstalledEntryRequested, this,
            &GMainWindow::OnGameListRemoveInstalledEntry);
    connect(game_list, &GameList::RemoveFileRequested, this, &GMainWindow::OnGameListRemoveFile);
    connect(game_list, &GameList::RemovePlayTimeRequested, this,
            &GMainWindow::OnGameListRemovePlayTimeData);
    connect(game_list, &GameList::DumpRomFSRequested, this, &GMainWindow::OnGameListDumpRomFS);
    connect(game_list, &GameList::VerifyIntegrityRequested, this,
            &GMainWindow::OnGameListVerifyIntegrity);
    connect(game_list, &GameList::CopyTIDRequested, this, &GMainWindow::OnGameListCopyTID);
    connect(game_list, &GameList::NavigateToGamedbEntryRequested, this,
            &GMainWindow::OnGameListNavigateToGamedbEntry);
    connect(game_list, &GameList::CreateShortcut, this, &GMainWindow::OnGameListCreateShortcut);
    connect(game_list, &GameList::CreateSteamShortcut, this,
            &GMainWindow::OnGameListCreateSteamShortcut);
    connect(game_list, &GameList::AddDirectory, this, &GMainWindow::OnGameListAddDirectory);
    connect(game_list_placeholder, &GameListPlaceholder::AddDirectory, this,
            &GMainWindow::OnGameListAddDirectory);
    connect(game_list, &GameList::ShowList, this, &GMainWindow::OnGameListShowList);
    connect(game_list, &GameList::PopulatingCompleted,
            [this] { multiplayer_state->UpdateGameList(game_list->GetModel()); });
    connect(game_list, &GameList::SaveConfig, this, &GMainWindow::OnSaveConfig);

    connect(game_list, &GameList::OpenPerGameGeneralRequested, this,
            &GMainWindow::OnGameListOpenPerGameProperties);

    connect(this, &GMainWindow::UpdateInstallProgress, this,
            &GMainWindow::IncrementInstallProgress);

    connect(this, &GMainWindow::EmulationStarting, render_window,
            &GRenderWindow::OnEmulationStarting);
    connect(this, &GMainWindow::EmulationStopping, render_window,
            &GRenderWindow::OnEmulationStopping);

    // Software Keyboard Applet
    connect(this, &GMainWindow::EmulationStarting, this, &GMainWindow::SoftwareKeyboardExit);
    connect(this, &GMainWindow::EmulationStopping, this, &GMainWindow::SoftwareKeyboardExit);

    connect(&status_bar_update_timer, &QTimer::timeout, this, &GMainWindow::UpdateStatusBar);

    connect(this, &GMainWindow::UpdateThemedIcons, multiplayer_state,
            &MultiplayerState::UpdateThemedIcons);
}

void GMainWindow::ConnectMenuEvents() {
    const auto connect_menu = [&]<typename Fn>(QAction* action, const Fn& event_fn) {
        connect(action, &QAction::triggered, this, event_fn);
        // Add actions to this window so that hiding menus in fullscreen won't disable them
        addAction(action);
        // Add actions to the render window so that they work outside of single window mode
        render_window->addAction(action);
    };

    // File
    connect_menu(ui->action_Load_File, &GMainWindow::OnMenuLoadFile);
    connect_menu(ui->action_Load_Folder, &GMainWindow::OnMenuLoadFolder);
    connect_menu(ui->action_Install_File_NAND, &GMainWindow::OnMenuInstallToNAND);
    connect_menu(ui->action_Exit, &QMainWindow::close);
    connect_menu(ui->action_Load_Amiibo, &GMainWindow::OnLoadAmiibo);
    connect_menu(ui->action_Export_Game, &GMainWindow::OnExportGame);

    // Emulation
    connect_menu(ui->action_Pause, &GMainWindow::OnPauseContinueGame);
    connect_menu(ui->action_Stop, &GMainWindow::OnStopGame);
    connect_menu(ui->action_Report_Compatibility, &GMainWindow::OnMenuReportCompatibility);
    connect_menu(ui->action_Open_Mods_Page, &GMainWindow::OnOpenModsPage);
    connect_menu(ui->action_Open_Quickstart_Guide, &GMainWindow::OnOpenQuickstartGuide);
    connect_menu(ui->action_Open_FAQ, &GMainWindow::OnOpenFAQ);
    {
        QAction* action_game_overlay = new QAction(tr("Show Game Overlay"), this);
        action_game_overlay->setShortcut(QKeySequence(Qt::Key_F1));
        action_game_overlay->setShortcutContext(Qt::WidgetWithChildrenShortcut);
        ui->menu_Emulation->addAction(action_game_overlay);
        connect_menu(action_game_overlay, &GMainWindow::OnShowGameOverlay);
    }
    connect_menu(ui->action_Restart, &GMainWindow::OnRestartGame);
    connect_menu(ui->action_Configure, &GMainWindow::OnConfigure);
    connect_menu(ui->action_Configure_Current_Game, &GMainWindow::OnConfigurePerGame);

    // View
    connect_menu(ui->action_Fullscreen, &GMainWindow::ToggleFullscreen);
    connect_menu(ui->action_Single_Window_Mode, &GMainWindow::ToggleWindowMode);
    connect_menu(ui->action_Display_Dock_Widget_Headers, &GMainWindow::OnDisplayTitleBars);
    connect_menu(ui->action_Show_Filter_Bar, &GMainWindow::OnToggleFilterBar);
    connect_menu(ui->action_Show_Status_Bar, &GMainWindow::OnToggleStatusBar);
    connect_menu(ui->action_Show_Folders_In_List, &GMainWindow::OnToggleFoldersInList);
    connect_menu(ui->action_Change_Interface_Mode, &GMainWindow::OnChangeInterfaceMode);

    connect_menu(ui->action_Reset_Window_Size_720, &GMainWindow::ResetWindowSize720);
    connect_menu(ui->action_Reset_Window_Size_800, &GMainWindow::ResetWindowSize800);
    connect_menu(ui->action_Reset_Window_Size_900, &GMainWindow::ResetWindowSize900);
    connect_menu(ui->action_Reset_Window_Size_1080, &GMainWindow::ResetWindowSize1080);
    ui->menu_Reset_Window_Size->addActions({ui->action_Reset_Window_Size_720,
                                            ui->action_Reset_Window_Size_800,
                                            ui->action_Reset_Window_Size_900,
                                            ui->action_Reset_Window_Size_1080});

    // Multiplayer
    connect(ui->action_View_Lobby, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnViewLobby);
    connect(ui->action_Start_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCreateRoom);
    connect(ui->action_Leave_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnCloseRoom);
    connect(ui->action_Connect_To_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnDirectConnectToRoom);
    connect(ui->action_Show_Room, &QAction::triggered, multiplayer_state,
            &MultiplayerState::OnOpenNetworkRoom);
    connect(multiplayer_state, &MultiplayerState::SaveConfig, this, &GMainWindow::OnSaveConfig);

    // Tools
    connect_menu(ui->action_Load_Album, &GMainWindow::OnAlbum);
    connect_menu(ui->action_Load_Cabinet_Nickname_Owner,
                 [this]() { OnCabinet(Service::NFP::CabinetMode::StartNicknameAndOwnerSettings); });
    connect_menu(ui->action_Load_Cabinet_Eraser,
                 [this]() { OnCabinet(Service::NFP::CabinetMode::StartGameDataEraser); });
    connect_menu(ui->action_Load_Cabinet_Restorer,
                 [this]() { OnCabinet(Service::NFP::CabinetMode::StartRestorer); });
    connect_menu(ui->action_Load_Cabinet_Formatter,
                 [this]() { OnCabinet(Service::NFP::CabinetMode::StartFormatter); });
    connect_menu(ui->action_Load_Mii_Edit, &GMainWindow::OnMiiEdit);
    connect_menu(ui->action_Open_Controller_Menu, &GMainWindow::OnOpenControllerMenu);
    connect_menu(ui->action_Load_Home_Menu, &GMainWindow::OnHomeMenu);
    connect_menu(ui->action_Capture_Screenshot, &GMainWindow::OnCaptureScreenshot);

    // TAS
    connect_menu(ui->action_TAS_Start, &GMainWindow::OnTasStartStop);
    connect_menu(ui->action_TAS_Record, &GMainWindow::OnTasRecord);
    connect_menu(ui->action_TAS_Reset, &GMainWindow::OnTasReset);
    connect_menu(ui->action_Configure_Tas, &GMainWindow::OnConfigureTas);

    // Custom features – Tools
    connect_menu(ui->action_Nintendo_Account, &GMainWindow::OnNintendoAccount);
    connect_menu(ui->action_Steam_Integration, &GMainWindow::OnSteamIntegration);

    // Help
    connect_menu(ui->action_Open_suyu_Folder, &GMainWindow::OnOpenSuyuFolder);
    connect_menu(ui->action_Verify_installed_contents, &GMainWindow::OnVerifyInstalledContents);
    connect_menu(ui->action_Install_Firmware, &GMainWindow::OnInstallFirmware);
    connect_menu(ui->action_Install_Keys, &GMainWindow::OnInstallDecryptionKeys);
    connect_menu(ui->action_Configure_External_Decryption,
                 &GMainWindow::OnConfigureExternalDecryption);
    connect_menu(ui->action_About, &GMainWindow::OnAbout);
    connect_menu(ui->action_Open_User_Manual, &GMainWindow::OnOpenUserManual);
}

void GMainWindow::UpdateMenuState() {
    const bool is_paused = emu_thread == nullptr || !emu_thread->IsRunning();
    const bool is_firmware_available = CheckFirmwarePresence();

    const std::array running_actions{
        ui->action_Stop,
        ui->action_Restart,
        ui->action_Configure_Current_Game,
        ui->action_Report_Compatibility,
        ui->action_Load_Amiibo,
        ui->action_Pause,
    };

    const std::array applet_actions{ui->action_Load_Album,
                                    ui->action_Load_Cabinet_Nickname_Owner,
                                    ui->action_Load_Cabinet_Eraser,
                                    ui->action_Load_Cabinet_Restorer,
                                    ui->action_Load_Cabinet_Formatter,
                                    ui->action_Load_Mii_Edit,
                                    ui->action_Open_Controller_Menu};

    for (QAction* action : running_actions) {
        action->setEnabled(emulation_running);
    }

    ui->action_Install_Firmware->setEnabled(!emulation_running);
    ui->action_Install_Keys->setVisible(true);
    ui->action_Install_Keys->setEnabled(!emulation_running);
    ui->action_Configure_External_Decryption->setEnabled(!emulation_running);

    for (QAction* action : applet_actions) {
        action->setEnabled(is_firmware_available && !emulation_running);
    }

    ui->action_Capture_Screenshot->setEnabled(emulation_running && !is_paused);

    if (emulation_running && is_paused) {
        ui->action_Pause->setText(tr("&Continue"));
    } else {
        ui->action_Pause->setText(tr("&Pause"));
    }

    multiplayer_state->UpdateNotificationStatus();
}

void GMainWindow::OnDisplayTitleBars(bool show) {
    QList<QDockWidget*> widgets = findChildren<QDockWidget*>();

    if (show) {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(nullptr);
            if (old != nullptr)
                delete old;
        }
    } else {
        for (QDockWidget* widget : widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(new QWidget());
            if (old != nullptr)
                delete old;
        }
    }
}

#ifdef __unix__
void GMainWindow::SetupPrepareForSleep() {
    auto bus = QDBusConnection::systemBus();
    if (bus.isConnected()) {
        const bool success = bus.connect(
            QStringLiteral("org.freedesktop.login1"), QStringLiteral("/org/freedesktop/login1"),
            QStringLiteral("org.freedesktop.login1.Manager"), QStringLiteral("PrepareForSleep"),
            QStringLiteral("b"), this, SLOT(OnPrepareForSleep(bool)));

        if (!success) {
            LOG_WARNING(Frontend, "Couldn't register PrepareForSleep signal");
        }
    } else {
        LOG_WARNING(Frontend, "QDBusConnection system bus is not connected");
    }
}
#endif // __unix__

void GMainWindow::OnPrepareForSleep(bool prepare_sleep) {
    if (emu_thread == nullptr) {
        return;
    }

    if (prepare_sleep) {
        if (emu_thread->IsRunning()) {
            auto_paused = true;
            OnPauseGame();
        }
    } else {
        if (!emu_thread->IsRunning() && auto_paused) {
            auto_paused = false;
            OnStartGame();
        }
    }
}

#ifdef __unix__
std::array<int, 3> GMainWindow::sig_interrupt_fds{0, 0, 0};

void GMainWindow::SetupSigInterrupts() {
    if (sig_interrupt_fds[2] == 1) {
        return;
    }
    socketpair(AF_UNIX, SOCK_STREAM, 0, sig_interrupt_fds.data());
    sig_interrupt_fds[2] = 1;

    struct sigaction sa;
    sa.sa_handler = &GMainWindow::HandleSigInterrupt;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESETHAND;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    sig_interrupt_notifier = new QSocketNotifier(sig_interrupt_fds[1], QSocketNotifier::Read, this);
    connect(sig_interrupt_notifier, &QSocketNotifier::activated, this,
            &GMainWindow::OnSigInterruptNotifierActivated);
    connect(this, &GMainWindow::SigInterrupt, this, &GMainWindow::close);
}

void GMainWindow::HandleSigInterrupt(int sig) {
    if (sig == SIGINT) {
        _exit(1);
    }

    // Calling into Qt directly from a signal handler is not safe,
    // so wake up a QSocketNotifier with this hacky write call instead.
    char a = 1;
    int ret = write(sig_interrupt_fds[0], &a, sizeof(a));
    (void)ret;
}

void GMainWindow::OnSigInterruptNotifierActivated() {
    sig_interrupt_notifier->setEnabled(false);

    char a;
    int ret = read(sig_interrupt_fds[1], &a, sizeof(a));
    (void)ret;

    sig_interrupt_notifier->setEnabled(true);

    emit SigInterrupt();
}
#endif // __unix__

void GMainWindow::PreventOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
#elif defined(HAVE_SDL2)
    SDL_DisableScreenSaver();
#endif
}

void GMainWindow::AllowOSSleep() {
#ifdef _WIN32
    SetThreadExecutionState(ES_CONTINUOUS);
#elif defined(HAVE_SDL2)
    SDL_EnableScreenSaver();
#endif
}

bool GMainWindow::LoadROM(const QString& filename, Service::AM::FrontendAppletParameters params) {
    if (Loader::AppLoader_NRO::IdentifyType(
            Core::GetGameFileFromPath(vfs, filename.toStdString())) != Loader::FileType::NRO) {
        if (!CheckFirmwarePresence()) {
            const auto response = QMessageBox::question(
                this, tr("Firmware Not Found"),
                tr("Nintendo Switch firmware was not detected for this launch.\n\n"
                   "If you already installed firmware, this may be a detection mismatch.\n"
                   "Choose Continue to attempt launch anyway, or Cancel to stop."),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (response != QMessageBox::Yes) {
                return false;
            }
        }

        if (!ContentManager::AreKeysPresent()) {
            const auto response = QMessageBox::warning(
                this, tr("No Decryption Keys Detected"),
                tr("No local decryption keys were detected.\n\n"
                   "Install your keys via\n"
                   "Tools > Install Decryption Keys,\n"
                   "or configure an external decryption tool if you prefer.\n\n"
                   "If your games are already decrypted, choose Continue."),
                QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
            if (response != QMessageBox::Yes) {
                return false;
            }
        }
    }

    // Shutdown previous session if the emu thread is still active...
    if (emu_thread != nullptr) {
        ShutdownGame();
    }

    if (!render_window->InitRenderTarget()) {
        return false;
    }

    system->SetFilesystem(vfs);

    if (params.launch_type == Service::AM::LaunchType::FrontendInitiated) {
        system->GetUserChannel().clear();
    }

    system->SetFrontendAppletSet({
        std::make_unique<QtAmiiboSettings>(*this), // Amiibo Settings
        (UISettings::values.controller_applet_disabled.GetValue() == true)
            ? nullptr
            : std::make_unique<QtControllerSelector>(*this), // Controller Selector
        std::make_unique<QtErrorDisplay>(*this),             // Error Display
        nullptr,                                             // Mii Editor
        nullptr,                                             // Parental Controls
        nullptr,                                             // Photo Viewer
        std::make_unique<QtProfileSelector>(*this),          // Profile Selector
        std::make_unique<QtSoftwareKeyboard>(*this),         // Software Keyboard
        std::make_unique<QtWebBrowser>(*this),               // Web Browser
        nullptr,                                             // Net Connect
    });

    const Core::SystemResultStatus result{
        system->Load(*render_window, filename.toStdString(), params)};

    const auto drd_callout = (UISettings::values.callout_flags.GetValue() &
                              static_cast<u32>(CalloutFlag::DRDDeprecation)) == 0;

    if (result == Core::SystemResultStatus::Success &&
        system->GetAppLoader().GetFileType() == Loader::FileType::DeconstructedRomDirectory &&
        drd_callout) {
        UISettings::values.callout_flags = UISettings::values.callout_flags.GetValue() |
                                           static_cast<u32>(CalloutFlag::DRDDeprecation);
        QMessageBox::warning(
            this, tr("Warning Outdated Game Format"),
            tr("You are using the deconstructed ROM directory format for this game, which is an "
               "outdated format that has been superseded by others such as NCA, NAX, XCI, or "
               "NSP. Deconstructed ROM directories lack icons, metadata, and update "
               "support.<br><br>For an explanation of the various Switch formats suyu supports, <a "
               "href='https://suyu-emu.github.io/website/'>check out our website</a>. This message will not be shown again."));
    }

    if (result != Core::SystemResultStatus::Success) {
        switch (result) {
        case Core::SystemResultStatus::ErrorGetLoader:
            LOG_CRITICAL(Frontend, "Failed to obtain loader for {}!", filename.toStdString());
            QMessageBox::critical(this, tr("Error while loading ROM!"),
                                  tr("The ROM format is not supported."));
            break;
        case Core::SystemResultStatus::ErrorVideoCore:
            QMessageBox::critical(
                this, tr("An error occurred initializing the video core."),
                tr("suyu has encountered an error while running the video core. "
                   "This is usually caused by outdated GPU drivers, including integrated ones. "
                   "Please see the log for more details. "
                   "For more information, please visit the <a href='https://suyu-emu.github.io/website/'>"
                   "suyu website</a>. "));
            break;
        default:
            if (result > Core::SystemResultStatus::ErrorLoader) {
                const u16 loader_id = static_cast<u16>(Core::SystemResultStatus::ErrorLoader);
                const u16 error_id = static_cast<u16>(result) - loader_id;
                const std::string error_code = fmt::format("({:04X}-{:04X})", loader_id, error_id);
                LOG_CRITICAL(Frontend, "Failed to load ROM! {}", error_code);

                const auto title =
                    tr("Error while loading ROM! %1", "%1 signifies a numeric error code.")
                        .arg(QString::fromStdString(error_code));
                const auto description =
                    tr("%1<br>Please visit <a href='https://suyu-emu.github.io/website/'>the "
                       "suyu website</a> for help redumping your files. You can also refer "
                       "to the suyu Discord for help.",
                       "%1 signifies an error string.")
                        .arg(QString::fromStdString(
                            GetResultStatusString(static_cast<Loader::ResultStatus>(error_id))));

                QMessageBox::critical(this, title, description);
            } else {
                QMessageBox::critical(
                    this, tr("Error while loading ROM!"),
                    tr("An unknown error occurred. Please see the log for more details."));
            }
            break;
        }
        return false;
    }
    current_game_path = filename;
    return true;
}

bool GMainWindow::SelectAndSetCurrentUser(
    const Core::Frontend::ProfileSelectParameters& parameters) {
    QtProfileSelectionDialog dialog(*system, this, parameters);
    dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                          Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
    dialog.setWindowModality(Qt::WindowModal);

    if (dialog.exec() == QDialog::Rejected) {
        return false;
    }

    Settings::values.current_user = dialog.GetIndex();
    return true;
}

void GMainWindow::ConfigureFilesystemProvider(const std::string& filepath) {
    // Ensure all NCAs are registered before launching the game
    const auto file = vfs->OpenFile(filepath, FileSys::OpenMode::Read);
    if (!file) {
        return;
    }

    auto loader = Loader::GetLoader(*system, file);
    if (!loader) {
        return;
    }

    const auto file_type = loader->GetFileType();
    if (file_type == Loader::FileType::Unknown || file_type == Loader::FileType::Error) {
        return;
    }

    u64 program_id = 0;
    const auto res2 = loader->ReadProgramId(program_id);
    if (res2 == Loader::ResultStatus::Success && file_type == Loader::FileType::NCA) {
        provider->AddEntry(FileSys::TitleType::Application,
                           FileSys::GetCRTypeFromNCAType(FileSys::NCA{file}.GetType()), program_id,
                           file);
    } else if (res2 == Loader::ResultStatus::Success &&
               (file_type == Loader::FileType::XCI || file_type == Loader::FileType::NSP)) {
        const auto nsp = file_type == Loader::FileType::NSP
                             ? std::make_shared<FileSys::NSP>(file)
                             : FileSys::XCI{file}.GetSecurePartitionNSP();
        for (const auto& title : nsp->GetNCAs()) {
            for (const auto& entry : title.second) {
                provider->AddEntry(entry.first.first, entry.first.second, title.first,
                                   entry.second->GetBaseFile());
            }
        }
    }
}

void GMainWindow::BootGame(const QString& filename, Service::AM::FrontendAppletParameters params,
                           StartGameType type) try {
    LOG_INFO(Frontend, "suyu starting...");

    if (params.program_id == 0 ||
        params.program_id > static_cast<u64>(Service::AM::AppletProgramId::MaxProgramId)) {
        StoreRecentFile(filename); // Put the filename on top of the list
    }

    // Save configurations
    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    config->SaveAllValues();

    u64 title_id{0};

    last_filename_booted = filename;

    ConfigureFilesystemProvider(filename.toStdString());
    const auto v_file = Core::GetGameFileFromPath(vfs, filename.toUtf8().constData());
    const auto loader = Loader::GetLoader(*system, v_file, params.program_id, params.program_index);

    if (loader != nullptr && loader->ReadProgramId(title_id) == Loader::ResultStatus::Success &&
        type == StartGameType::Normal) {
        // Load per game settings
        const auto file_path =
            std::filesystem::path{Common::U16StringFromBuffer(filename.utf16(), filename.size())};
        const auto config_file_name = title_id == 0
                                          ? Common::FS::PathToUTF8String(file_path.filename())
                                          : fmt::format("{:016X}", title_id);
        QtConfig per_game_config(config_file_name, Config::ConfigType::PerGameConfig);
        system->HIDCore().ReloadInputDevices();
        system->ApplySettings();
    }

    Settings::LogSettings();

    if (UISettings::values.select_user_on_boot && !user_flag_cmd_line) {
        const Core::Frontend::ProfileSelectParameters parameters{
            .mode = Service::AM::Frontend::UiMode::UserSelector,
            .invalid_uid_list = {},
            .display_options = {},
            .purpose = Service::AM::Frontend::UserSelectionPurpose::General,
        };
        if (SelectAndSetCurrentUser(parameters) == false) {
            return;
        }
    }

    // If the user specifies -u (successfully) on the cmd line, don't prompt for a user on first
    // game startup only. If the user stops emulation and starts a new one, go back to the expected
    // behavior of asking.
    user_flag_cmd_line = false;

    if (Settings::values.renderer_backend.GetValue() == Settings::RendererBackend::Vulkan) {
        const auto vulkan_device_index =
            static_cast<std::size_t>(Settings::values.vulkan_device.GetValue());
        if (vulkan_device_index < vk_device_records.size() &&
            vk_device_records[vulkan_device_index].name.find("Intel") != std::string::npos) {
            LOG_INFO(Frontend,
                     "Selected Vulkan device '{}' is Intel; keeping the configured Vulkan backend "
                     "and allowing renderer initialization to report any real device error.",
                        vk_device_records[vulkan_device_index].name);
        }
    }

    // Stop background game-list scanning before boot to avoid sharing Core::System
    // with recursive directory scans while the launch path is initializing.
    game_list->CancelPopulate();

    if (!LoadROM(filename, params)) {
        return;
    }

    system->SetShuttingDown(false);
    game_list->setDisabled(true);
    game_list->hide();
    game_list_placeholder->hide();
    if (gamer_env_) {
        gamer_env_->hide();
    }
    if (programmer_env_) {
        programmer_env_->hide();
    }
    if (hacker_env_dock_) {
        hacker_env_dock_->hide();
    }

    // Create the emulation thread and wire all UI/error handlers before it starts.
    // This avoids a race where the thread exits before the fatal-error connection is live,
    // which can leave the loading screen visible indefinitely.
    emu_thread = std::make_unique<EmuThread>(*system);
    emit EmulationStarting(emu_thread.get());
    emulation_running = true;

    // Register an ExecuteProgram callback such that Core can execute a sub-program
    system->RegisterExecuteProgramCallback(
        [this](std::size_t program_index_) { render_window->ExecuteProgram(program_index_); });

    system->RegisterExitCallback([this] {
        // Guard against the thread already being torn down (e.g. double-exit callbacks).
        if (emu_thread) {
            emu_thread->ForceStop();
        }
        render_window->Exit();
    });

    connect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);
    connect(render_window, &GRenderWindow::MouseActivity, this, &GMainWindow::OnMouseActivity);
    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views
    // before the CPU continues
    connect(emu_thread.get(), &EmuThread::DebugModeEntered, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeEntered, Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), &EmuThread::DebugModeLeft, waitTreeWidget,
            &WaitTreeWidget::OnDebugModeLeft, Qt::BlockingQueuedConnection);

    connect(emu_thread.get(), &EmuThread::LoadProgress, loading_screen,
            &LoadingScreen::OnLoadProgress, Qt::QueuedConnection);

    connect(emu_thread.get(), &EmuThread::FatalError, this,
            [this](const QString& msg) {
                LOG_CRITICAL(Frontend, "Emulation thread crashed: {}", msg.toStdString());
                QMessageBox::critical(this, tr("Emulation Crashed"),
                                      tr("The emulation thread encountered a fatal error:\n\n%1\n\n"
                                         "The game has been stopped.")
                                          .arg(msg));
                // Attempt graceful cleanup; skip confirm dialog since emulation already died.
                if (emulation_running) {
                    play_time_manager->Stop();
                    OnShutdownBegin();
                    OnEmulationStopTimeExpired();
                    OnEmulationStopped();
                }
            },
            Qt::QueuedConnection);

    // Update the GUI
    UpdateStatusButtons();
    if (ui->action_Single_Window_Mode->isChecked()) {
        game_list->hide();
        game_list_placeholder->hide();
    }
    status_bar_update_timer.start(500);
    renderer_status_button->setDisabled(true);

    if (UISettings::values.hide_mouse || Settings::values.mouse_panning) {
        render_window->installEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, true);
    }

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }

    render_window->InitializeCamera();

    std::string title_name;
    std::string title_version;
    const auto res = system->GetGameName(title_name);

    const auto metadata = [this, title_id] {
        const FileSys::PatchManager pm(title_id, system->GetFileSystemController(),
                                       system->GetContentProvider());
        return pm.GetControlMetadata();
    }();
    if (metadata.first != nullptr) {
        title_version = metadata.first->GetVersionString();
        title_name = metadata.first->GetApplicationName();
    }
    if (res != Loader::ResultStatus::Success || title_name.empty()) {
        title_name = Common::FS::PathToUTF8String(
            std::filesystem::path{Common::U16StringFromBuffer(filename.utf16(), filename.size())}
                .filename());
    }
    const bool is_64bit = system->Kernel().ApplicationProcess()->Is64Bit();
    const auto instruction_set_suffix = is_64bit ? tr("(64-bit)") : tr("(32-bit)");
    title_name = tr("%1 %2", "%1 is the title name. %2 indicates if the title is 64-bit or 32-bit")
                     .arg(QString::fromStdString(title_name), instruction_set_suffix)
                     .toStdString();
    LOG_INFO(Frontend, "Booting game: {:016X} | {} | {}", title_id, title_name, title_version);
    const auto gpu_vendor = system->GPU().Renderer().GetDeviceVendor();
    UpdateWindowTitle(title_name, title_version, gpu_vendor);

    loading_screen->Prepare(system->GetAppLoader());
    loading_screen->setGeometry(ui->emulationPage->rect());
    if (ui->action_Single_Window_Mode->isChecked()) {
        render_window->show();
    }
    loading_screen->raise();
    loading_screen->show();

    // Start emulation only after the loading UI and failure handlers are fully armed.
    emu_thread->start();

    // emulation_running was already set to true above so early shutdown paths remain valid.
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    }
    OnStartGame();
} catch (const std::exception& e) {
    LOG_CRITICAL(Frontend, "Unhandled exception while starting game '{}': {}",
                 filename.toStdString(), e.what());
    if (emulation_running) {
        // Thread was already started; perform full cleanup.
        ShutdownGame();
    } else if (game_list) {
        game_list->setDisabled(false);
    }
    QMessageBox::critical(this, tr("Launch failed"),
                          tr("An internal error occurred while starting the game:\n%1")
                              .arg(QString::fromUtf8(e.what())));
} catch (...) {
    LOG_CRITICAL(Frontend, "Unknown unhandled exception while starting game '{}'",
                 filename.toStdString());
    if (emulation_running) {
        ShutdownGame();
    } else if (game_list) {
        game_list->setDisabled(false);
    }
    QMessageBox::critical(this, tr("Launch failed"),
                          tr("An unknown internal error occurred while starting the game."));
}

void GMainWindow::BootGameFromList(const QString& filename, StartGameType with_config) {
    BootGame(filename, ApplicationAppletParameters(), with_config);
}

bool GMainWindow::OnShutdownBegin() {
    if (!emulation_running) {
        return false;
    }

    if (ui->action_Fullscreen->isChecked()) {
        HideFullscreen();
    }

    AllowOSSleep();

    // Disable unlimited frame rate
    Settings::values.use_speed_limit.SetValue(true);

    if (system->IsShuttingDown()) {
        return false;
    }

    system->SetShuttingDown(true);
    discord_rpc->Pause();

    RequestGameExit();
    emu_thread->disconnect();
    emu_thread->SetRunning(true);

    emit EmulationStopping();

    int shutdown_time = 1000;

    if (system->DebuggerEnabled()) {
        shutdown_time = 0;
    } else if (system->GetExitLocked()) {
        shutdown_time = 5000;
    }

    shutdown_timer.setSingleShot(true);
    shutdown_timer.start(shutdown_time);
    connect(&shutdown_timer, &QTimer::timeout, this, &GMainWindow::OnEmulationStopTimeExpired);
    connect(emu_thread.get(), &QThread::finished, this, &GMainWindow::OnEmulationStopped);

    // Disable everything to prevent anything from being triggered here
    ui->action_Pause->setEnabled(false);
    ui->action_Restart->setEnabled(false);
    ui->action_Stop->setEnabled(false);

    return true;
}

void GMainWindow::OnShutdownBeginDialog() {
    shutdown_dialog = new OverlayDialog(this, *system, QString{}, tr("Closing software..."),
                                        QString{}, QString{}, Qt::AlignHCenter | Qt::AlignVCenter);
    shutdown_dialog->open();
}

void GMainWindow::OnEmulationStopTimeExpired() {
    if (emu_thread) {
        emu_thread->ForceStop();
    }
}

void GMainWindow::OnEmulationStopped() {
    shutdown_timer.stop();
    if (emu_thread) {
        emu_thread->disconnect();
        emu_thread->wait();
        emu_thread.reset();
    }

    if (shutdown_dialog) {
        shutdown_dialog->deleteLater();
        shutdown_dialog = nullptr;
    }

    emulation_running = false;

    discord_rpc->Update();

#ifdef __unix__
    Common::Linux::StopGamemode();
#endif

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, &GRenderWindow::Closed, this, &GMainWindow::OnStopGame);

    // Update the GUI
    UpdateMenuState();

    render_window->hide();
    loading_screen->hide();
    loading_screen->Clear();
    if (current_mode_ == AppMode::Hacker) {
        if (game_list->IsEmpty()) {
            game_list_placeholder->show();
        } else {
            game_list->show();
        }
        game_list->SetFilterFocus();
    } else if (current_mode_ == AppMode::Gamer && gamer_env_) {
        gamer_env_->show();
        gamer_env_->RefreshGameGrid();
    } else if (current_mode_ == AppMode::Programmer && programmer_env_) {
        programmer_env_->show();
    }
    tas_label->clear();
    input_subsystem->GetTas()->Stop();
    OnTasStateChanged();
    render_window->FinalizeCamera();

    system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::None);

    // Enable all controllers
    system->HIDCore().SetSupportedStyleTag({Core::HID::NpadStyleSet::All});

    render_window->removeEventFilter(render_window);
    render_window->setAttribute(Qt::WA_Hover, false);

    UpdateWindowTitle();

    // Disable status bar updates
    status_bar_update_timer.stop();
    shader_building_label->setVisible(false);
    res_scale_label->setVisible(false);
    emu_speed_label->setVisible(false);
    game_fps_label->setVisible(false);
    emu_frametime_label->setVisible(false);
    renderer_status_button->setEnabled(!UISettings::values.has_broken_vulkan);

    if (!firmware_label->text().isEmpty()) {
        firmware_label->setVisible(true);
    }

    current_game_path.clear();

    // When closing the game, destroy the GLWindow to clear the context after the game is closed
    render_window->ReleaseRenderTarget();

    // Enable game list
    game_list->setEnabled(true);

    Settings::RestoreGlobalState(system->IsPoweredOn());
    system->HIDCore().ReloadInputDevices();
    UpdateStatusButtons();
}

void GMainWindow::ShutdownGame() {
    if (!emulation_running) {
        return;
    }

    play_time_manager->Stop();
    OnShutdownBegin();
    OnEmulationStopTimeExpired();
    OnEmulationStopped();
}

void GMainWindow::StoreRecentFile(const QString& filename) {
    UISettings::values.recent_files.prepend(filename);
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void GMainWindow::UpdateRecentFiles() {
    const int num_recent_files =
        std::min(static_cast<int>(UISettings::values.recent_files.size()), max_recent_files_item);

    for (int i = 0; i < num_recent_files; i++) {
        const QString text = QStringLiteral("&%1. %2").arg(i + 1).arg(
            QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Enable the recent files menu if the list isn't empty
    ui->menu_recent_files->setEnabled(num_recent_files != 0);
}

void GMainWindow::OnGameListLoadFile(QString game_path, u64 program_id) {
    auto params = ApplicationAppletParameters();
    params.program_id = program_id;

    BootGame(game_path, params);
}

void GMainWindow::OnGameListOpenFolder(u64 program_id, GameListOpenTarget target,
                                       const std::string& game_path) {
    std::filesystem::path path;
    QString open_target;

    const auto [user_save_size, device_save_size] = [this, &game_path, &program_id] {
        const FileSys::PatchManager pm{program_id, system->GetFileSystemController(),
                                       system->GetContentProvider()};
        const auto control = pm.GetControlMetadata().first;
        if (control != nullptr) {
            return std::make_pair(control->GetDefaultNormalSaveSize(),
                                  control->GetDeviceSaveDataSize());
        } else {
            const auto file = Core::GetGameFileFromPath(vfs, game_path);
            const auto loader = Loader::GetLoader(*system, file);

            FileSys::NACP nacp{};
            loader->ReadControlData(nacp);
            return std::make_pair(nacp.GetDefaultNormalSaveSize(), nacp.GetDeviceSaveDataSize());
        }
    }();

    const bool has_user_save{user_save_size > 0};
    const bool has_device_save{device_save_size > 0};

    ASSERT_MSG(has_user_save != has_device_save, "Game uses both user and device savedata?");

    switch (target) {
    case GameListOpenTarget::SaveData: {
        open_target = tr("Save Data");
        const auto nand_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir);
        auto vfs_nand_dir =
            vfs->OpenDirectory(Common::FS::PathToUTF8String(nand_dir), FileSys::OpenMode::Read);

        if (has_user_save) {
            // User save data
            const auto select_profile = [this] {
                const Core::Frontend::ProfileSelectParameters parameters{
                    .mode = Service::AM::Frontend::UiMode::UserSelector,
                    .invalid_uid_list = {},
                    .display_options = {},
                    .purpose = Service::AM::Frontend::UserSelectionPurpose::General,
                };
                QtProfileSelectionDialog dialog(*system, this, parameters);
                dialog.setWindowFlags(Qt::Dialog | Qt::CustomizeWindowHint | Qt::WindowTitleHint |
                                      Qt::WindowSystemMenuHint | Qt::WindowCloseButtonHint);
                dialog.setWindowModality(Qt::WindowModal);

                if (dialog.exec() == QDialog::Rejected) {
                    return -1;
                }

                return dialog.GetIndex();
            };

            const auto index = select_profile();
            if (index == -1) {
                return;
            }

            const auto user_id =
                system->GetProfileManager().GetUser(static_cast<std::size_t>(index));
            ASSERT(user_id);

            const auto user_save_data_path = FileSys::SaveDataFactory::GetFullPath(
                {}, vfs_nand_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account,
                program_id, user_id->AsU128(), 0);

            path = Common::FS::ConcatPathSafe(nand_dir, user_save_data_path);
        } else {
            // Device save data
            const auto device_save_data_path = FileSys::SaveDataFactory::GetFullPath(
                {}, vfs_nand_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Account,
                program_id, {}, 0);

            path = Common::FS::ConcatPathSafe(nand_dir, device_save_data_path);
        }

        if (!Common::FS::CreateDirs(path)) {
            LOG_ERROR(Frontend, "Unable to create the directories for save data");
        }

        break;
    }
    case GameListOpenTarget::ModData: {
        open_target = tr("Mod Data");
        path = Common::FS::GetSuyuPath(Common::FS::SuyuPath::LoadDir) /
               fmt::format("{:016X}", program_id);
        break;
    }
    default:
        LOG_ERROR(Frontend, "Open folder target not supported: {}", static_cast<int>(target));
        return;
    }

    const QString qpath = QString::fromStdString(Common::FS::PathToUTF8String(path));
    const QDir dir(qpath);
    if (!dir.exists()) {
        QMessageBox::warning(this, tr("Error Opening %1 Folder").arg(open_target),
                             tr("Folder does not exist!"));
        return;
    }
    LOG_INFO(Frontend, "Opening {} path for program_id={:016x}", open_target.toStdString(),
             program_id);
    QDesktopServices::openUrl(QUrl::fromLocalFile(qpath));
}

void GMainWindow::OnTransferableShaderCacheOpenFile(u64 program_id) {
    const auto shader_cache_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ShaderDir);
    const auto shader_cache_folder_path{shader_cache_dir / fmt::format("{:016x}", program_id)};
    if (!Common::FS::CreateDirs(shader_cache_folder_path)) {
        QMessageBox::warning(this, tr("Error Opening Transferable Shader Cache"),
                             tr("Failed to create the shader cache directory for this title."));
        return;
    }
    const auto shader_path_string{Common::FS::PathToUTF8String(shader_cache_folder_path)};
    const auto qt_shader_cache_path = QString::fromStdString(shader_path_string);
    QDesktopServices::openUrl(QUrl::fromLocalFile(qt_shader_cache_path));
}


static bool RomFSRawCopy(size_t total_size, size_t& read_size, QProgressDialog& dialog,
                         const FileSys::VirtualDir& src, const FileSys::VirtualDir& dest,
                         bool full) {
    if (src == nullptr || dest == nullptr || !src->IsReadable() || !dest->IsWritable())
        return false;
    if (dialog.wasCanceled())
        return false;

    std::vector<u8> buffer(CopyBufferSize);
    auto last_timestamp = std::chrono::steady_clock::now();

    const auto QtRawCopy = [&](const FileSys::VirtualFile& src_file,
                               const FileSys::VirtualFile& dest_file) {
        if (src_file == nullptr || dest_file == nullptr) {
            return false;
        }
        if (!dest_file->Resize(src_file->GetSize())) {
            return false;
        }

        for (std::size_t i = 0; i < src_file->GetSize(); i += buffer.size()) {
            if (dialog.wasCanceled()) {
                dest_file->Resize(0);
                return false;
            }

            using namespace std::literals::chrono_literals;
            const auto new_timestamp = std::chrono::steady_clock::now();

            if ((new_timestamp - last_timestamp) > 33ms) {
                last_timestamp = new_timestamp;
                dialog.setValue(
                    static_cast<int>(std::min(read_size, total_size) * 100 / total_size));
                QCoreApplication::processEvents();
            }

            const auto read = src_file->Read(buffer.data(), buffer.size(), i);
            dest_file->Write(buffer.data(), read, i);

            read_size += read;
        }

        return true;
    };

    if (full) {
        for (const auto& file : src->GetFiles()) {
            const auto out = VfsDirectoryCreateFileWrapper(dest, file->GetName());
            if (!QtRawCopy(file, out))
                return false;
        }
    }

    for (const auto& dir : src->GetSubdirectories()) {
        const auto out = dest->CreateSubdirectory(dir->GetName());
        if (!RomFSRawCopy(total_size, read_size, dialog, dir, out, full))
            return false;
    }

    return true;
}

QString GMainWindow::GetGameListErrorRemoving(InstalledEntryType type) const {
    switch (type) {
    case InstalledEntryType::Game:
        return tr("Error Removing Contents");
    case InstalledEntryType::Update:
        return tr("Error Removing Update");
    case InstalledEntryType::AddOnContent:
        return tr("Error Removing DLC");
    default:
        return QStringLiteral("Error Removing <Invalid Type>");
    }
}
void GMainWindow::OnGameListRemoveInstalledEntry(u64 program_id, InstalledEntryType type) {
    const QString entry_question = [type] {
        switch (type) {
        case InstalledEntryType::Game:
            return tr("Remove Installed Game Contents?");
        case InstalledEntryType::Update:
            return tr("Remove Installed Game Update?");
        case InstalledEntryType::AddOnContent:
            return tr("Remove Installed Game DLC?");
        default:
            return QStringLiteral("Remove Installed Game <Invalid Type>?");
        }
    }();

    if (!question(this, tr("Remove Entry"), entry_question, QMessageBox::Yes | QMessageBox::No,
                  QMessageBox::No)) {
        return;
    }

    switch (type) {
    case InstalledEntryType::Game:
        RemoveBaseContent(program_id, type);
        [[fallthrough]];
    case InstalledEntryType::Update:
        RemoveUpdateContent(program_id, type);
        if (type != InstalledEntryType::Game) {
            break;
        }
        [[fallthrough]];
    case InstalledEntryType::AddOnContent:
        RemoveAddOnContent(program_id, type);
        break;
    }
    Common::FS::RemoveDirRecursively(Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir) /
                                     "game_list");
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::RemoveBaseContent(u64 program_id, InstalledEntryType type) {
    const auto res =
        ContentManager::RemoveBaseContent(system->GetFileSystemController(), program_id);
    if (res) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the installed base game."));
    } else {
        QMessageBox::warning(
            this, GetGameListErrorRemoving(type),
            tr("The base game is not installed in the NAND and cannot be removed."));
    }
}

void GMainWindow::RemoveUpdateContent(u64 program_id, InstalledEntryType type) {
    const auto res = ContentManager::RemoveUpdate(system->GetFileSystemController(), program_id);
    if (res) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the installed update."));
    } else {
        QMessageBox::warning(this, GetGameListErrorRemoving(type),
                             tr("There is no update installed for this title."));
    }
}

void GMainWindow::RemoveAddOnContent(u64 program_id, InstalledEntryType type) {
    const size_t count = ContentManager::RemoveAllDLC(*system, program_id);
    if (count == 0) {
        QMessageBox::warning(this, GetGameListErrorRemoving(type),
                             tr("There are no DLC installed for this title."));
        return;
    }

    QMessageBox::information(this, tr("Successfully Removed"),
                             tr("Successfully removed %1 installed DLC.").arg(count));
}

void GMainWindow::OnGameListRemoveFile(u64 program_id, GameListRemoveTarget target,
                                       const std::string& game_path) {
    const QString question = [target] {
        switch (target) {
        case GameListRemoveTarget::GlShaderCache:
            return tr("Delete OpenGL Transferable Shader Cache?");
        case GameListRemoveTarget::VkShaderCache:
            return tr("Delete Vulkan Transferable Shader Cache?");
        case GameListRemoveTarget::AllShaderCache:
            return tr("Delete All Transferable Shader Caches?");
        case GameListRemoveTarget::CustomConfiguration:
            return tr("Remove Custom Game Configuration?");
        case GameListRemoveTarget::CacheStorage:
            return tr("Remove Cache Storage?");
        default:
            return QString{};
        }
    }();

    if (!GMainWindow::question(this, tr("Remove File"), question,
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No)) {
        return;
    }

    switch (target) {
    case GameListRemoveTarget::VkShaderCache:
        RemoveVulkanDriverPipelineCache(program_id);
        [[fallthrough]];
    case GameListRemoveTarget::GlShaderCache:
        RemoveTransferableShaderCache(program_id, target);
        break;
    case GameListRemoveTarget::AllShaderCache:
        RemoveAllTransferableShaderCaches(program_id);
        break;
    case GameListRemoveTarget::CustomConfiguration:
        RemoveCustomConfiguration(program_id, game_path);
        break;
    case GameListRemoveTarget::CacheStorage:
        RemoveCacheStorage(program_id);
        break;
    }
}

void GMainWindow::OnGameListRemovePlayTimeData(u64 program_id) {
    if (QMessageBox::question(this, tr("Remove Play Time Data"), tr("Reset play time?"),
                              QMessageBox::Yes | QMessageBox::No,
                              QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    play_time_manager->ResetProgramPlayTime(program_id);
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::RemoveTransferableShaderCache(u64 program_id, GameListRemoveTarget target) {
    const auto target_file_name = [target] {
        switch (target) {
        case GameListRemoveTarget::GlShaderCache:
            return "opengl.bin";
        case GameListRemoveTarget::VkShaderCache:
            return "vulkan.bin";
        default:
            return "";
        }
    }();
    const auto shader_cache_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ShaderDir);
    const auto shader_cache_folder_path = shader_cache_dir / fmt::format("{:016x}", program_id);
    const auto target_file = shader_cache_folder_path / target_file_name;

    if (!Common::FS::Exists(target_file)) {
        QMessageBox::warning(this, tr("Error Removing Transferable Shader Cache"),
                             tr("A shader cache for this title does not exist."));
        return;
    }
    if (Common::FS::RemoveFile(target_file)) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the transferable shader cache."));
    } else {
        QMessageBox::warning(this, tr("Error Removing Transferable Shader Cache"),
                             tr("Failed to remove the transferable shader cache."));
    }
}

void GMainWindow::RemoveVulkanDriverPipelineCache(u64 program_id) {
    static constexpr std::string_view target_file_name = "vulkan_pipelines.bin";

    const auto shader_cache_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ShaderDir);
    const auto shader_cache_folder_path = shader_cache_dir / fmt::format("{:016x}", program_id);
    const auto target_file = shader_cache_folder_path / target_file_name;

    if (!Common::FS::Exists(target_file)) {
        return;
    }
    if (!Common::FS::RemoveFile(target_file)) {
        QMessageBox::warning(this, tr("Error Removing Vulkan Driver Pipeline Cache"),
                             tr("Failed to remove the driver pipeline cache."));
    }
}

void GMainWindow::RemoveAllTransferableShaderCaches(u64 program_id) {
    const auto shader_cache_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ShaderDir);
    const auto program_shader_cache_dir = shader_cache_dir / fmt::format("{:016x}", program_id);

    if (!Common::FS::Exists(program_shader_cache_dir)) {
        QMessageBox::warning(this, tr("Error Removing Transferable Shader Caches"),
                             tr("A shader cache for this title does not exist."));
        return;
    }
    if (Common::FS::RemoveDirRecursively(program_shader_cache_dir)) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the transferable shader caches."));
    } else {
        QMessageBox::warning(this, tr("Error Removing Transferable Shader Caches"),
                             tr("Failed to remove the transferable shader cache directory."));
    }
}

void GMainWindow::RemoveCustomConfiguration(u64 program_id, const std::string& game_path) {
    const auto file_path = std::filesystem::path(Common::FS::ToU8String(game_path));
    const auto config_file_name =
        program_id == 0 ? Common::FS::PathToUTF8String(file_path.filename()).append(".ini")
                        : fmt::format("{:016X}.ini", program_id);
    const auto custom_config_file_path =
        Common::FS::GetSuyuPath(Common::FS::SuyuPath::ConfigDir) / "custom" / config_file_name;

    if (!Common::FS::Exists(custom_config_file_path)) {
        QMessageBox::warning(this, tr("Error Removing Custom Configuration"),
                             tr("A custom configuration for this title does not exist."));
        return;
    }

    if (Common::FS::RemoveFile(custom_config_file_path)) {
        QMessageBox::information(this, tr("Successfully Removed"),
                                 tr("Successfully removed the custom game configuration."));
    } else {
        QMessageBox::warning(this, tr("Error Removing Custom Configuration"),
                             tr("Failed to remove the custom game configuration."));
    }
}

void GMainWindow::RemoveCacheStorage(u64 program_id) {
    const auto nand_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir);
    auto vfs_nand_dir =
        vfs->OpenDirectory(Common::FS::PathToUTF8String(nand_dir), FileSys::OpenMode::Read);

    const auto cache_storage_path = FileSys::SaveDataFactory::GetFullPath(
        {}, vfs_nand_dir, FileSys::SaveDataSpaceId::User, FileSys::SaveDataType::Cache,
        0 /* program_id */, {}, 0);

    const auto path = Common::FS::ConcatPathSafe(nand_dir, cache_storage_path);

    // Not an error if it wasn't cleared.
    Common::FS::RemoveDirRecursively(path);
}

void GMainWindow::OnGameListDumpRomFS(u64 program_id, const std::string& game_path,
                                      DumpRomFSTarget target) {
    const auto failed = [this] {
        QMessageBox::warning(this, tr("RomFS Extraction Failed!"),
                             tr("There was an error copying the RomFS files or the user "
                                "cancelled the operation."));
    };

    const auto loader =
        Loader::GetLoader(*system, vfs->OpenFile(game_path, FileSys::OpenMode::Read));
    if (loader == nullptr) {
        failed();
        return;
    }

    FileSys::VirtualFile packed_update_raw{};
    loader->ReadUpdateRaw(packed_update_raw);

    const auto& installed = system->GetContentProvider();

    u64 title_id{};
    u8 raw_type{};
    if (!SelectRomFSDumpTarget(installed, program_id, &title_id, &raw_type)) {
        failed();
        return;
    }

    const auto type = static_cast<FileSys::ContentRecordType>(raw_type);
    const auto base_nca = installed.GetEntry(title_id, type);
    if (!base_nca) {
        failed();
        return;
    }

    const FileSys::NCA update_nca{packed_update_raw, nullptr};
    if (type != FileSys::ContentRecordType::Program ||
        update_nca.GetStatus() != Loader::ResultStatus::ErrorMissingBKTRBaseRomFS ||
        update_nca.GetTitleId() != FileSys::GetUpdateTitleID(title_id)) {
        packed_update_raw = {};
    }

    const auto base_romfs = base_nca->GetRomFS();
    const auto dump_dir =
        target == DumpRomFSTarget::Normal
            ? Common::FS::GetSuyuPath(Common::FS::SuyuPath::DumpDir)
            : Common::FS::GetSuyuPath(Common::FS::SuyuPath::SDMCDir) / "atmosphere" / "contents";
    const auto romfs_dir = fmt::format("{:016X}/romfs", title_id);

    const auto path = Common::FS::PathToUTF8String(dump_dir / romfs_dir);

    const FileSys::PatchManager pm{title_id, system->GetFileSystemController(), installed};
    auto romfs = pm.PatchRomFS(base_nca.get(), base_romfs, type, packed_update_raw, false);

    const auto out = VfsFilesystemCreateDirectoryWrapper(vfs, path, FileSys::OpenMode::ReadWrite);

    if (out == nullptr) {
        failed();
        vfs->DeleteDirectory(path);
        return;
    }

    bool ok = false;
    const QStringList selections{tr("Full"), tr("Skeleton")};
    const auto res = QInputDialog::getItem(
        this, tr("Select RomFS Dump Mode"),
        tr("Please select the how you would like the RomFS dumped.<br>Full will copy all of the "
           "files into the new directory while <br>skeleton will only create the directory "
           "structure."),
        selections, 0, false, &ok);
    if (!ok) {
        failed();
        vfs->DeleteDirectory(path);
        return;
    }

    const auto extracted = FileSys::ExtractRomFS(romfs);
    if (extracted == nullptr) {
        failed();
        return;
    }

    const auto full = res == selections.constFirst();

    // The expected required space is the size of the RomFS + 1 GiB
    const auto minimum_free_space = romfs->GetSize() + 0x40000000;

    if (full && Common::FS::GetFreeSpaceSize(path) < minimum_free_space) {
        QMessageBox::warning(this, tr("RomFS Extraction Failed!"),
                             tr("There is not enough free space at %1 to extract the RomFS. Please "
                                "free up space or select a different dump directory at "
                                "Emulation > Configure > System > Filesystem > Dump Root")
                                 .arg(QString::fromStdString(path)));
        return;
    }

    QProgressDialog progress(tr("Extracting RomFS..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    size_t read_size = 0;

    if (RomFSRawCopy(romfs->GetSize(), read_size, progress, extracted, out, full)) {
        progress.close();
        QMessageBox::information(this, tr("RomFS Extraction Succeeded!"),
                                 tr("The operation completed successfully."));
        QDesktopServices::openUrl(QUrl::fromLocalFile(QString::fromStdString(path)));
    } else {
        progress.close();
        failed();
        vfs->DeleteDirectory(path);
    }
}

void GMainWindow::OnGameListVerifyIntegrity(const std::string& game_path) {
    const auto NotImplemented = [this] {
        QMessageBox::warning(this, tr("Integrity verification couldn't be performed!"),
                             tr("File contents were not checked for validity."));
    };

    QProgressDialog progress(tr("Verifying integrity..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    const auto QtProgressCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const auto result = ContentManager::VerifyGameContents(*system, game_path, QtProgressCallback);
    progress.close();
    switch (result) {
    case ContentManager::GameVerificationResult::Success:
        QMessageBox::information(this, tr("Integrity verification succeeded!"),
                                 tr("The operation completed successfully."));
        break;
    case ContentManager::GameVerificationResult::Failed:
        QMessageBox::critical(this, tr("Integrity verification failed!"),
                              tr("File contents may be corrupt."));
        break;
    case ContentManager::GameVerificationResult::NotImplemented:
        NotImplemented();
    }
}

void GMainWindow::OnGameListCopyTID(u64 program_id) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    clipboard->setText(QString::fromStdString(fmt::format("{:016X}", program_id)));
}

void GMainWindow::OnGameListNavigateToGamedbEntry(u64 program_id,
                                                  const CompatibilityList& compatibility_list) {
    const auto it = FindMatchingCompatibilityEntry(compatibility_list, program_id);

    QString directory;
    if (it != compatibility_list.end()) {
        directory = it->second.second;
    }

    QDesktopServices::openUrl(QUrl(QStringLiteral("https://suyu-emu.github.io/website/")));
}

bool GMainWindow::CreateShortcutLink(const std::filesystem::path& shortcut_path,
                                     const std::string& comment,
                                     const std::filesystem::path& icon_path,
                                     const std::filesystem::path& command,
                                     const std::string& arguments, const std::string& categories,
                                     const std::string& keywords, const std::string& name) try {
#if defined(__linux__) || defined(__FreeBSD__) // Linux and FreeBSD
    std::filesystem::path shortcut_path_full = shortcut_path / (name + ".desktop");
    std::ofstream shortcut_stream(shortcut_path_full, std::ios::binary | std::ios::trunc);
    if (!shortcut_stream.is_open()) {
        LOG_ERROR(Frontend, "Failed to create shortcut");
        return false;
    }
    // TODO: Migrate fmt::print to std::print in futures STD C++ 23.
    fmt::print(shortcut_stream, "[Desktop Entry]\n");
    fmt::print(shortcut_stream, "Type=Application\n");
    fmt::print(shortcut_stream, "Version=1.0\n");
    fmt::print(shortcut_stream, "Name={}\n", name);
    if (!comment.empty()) {
        fmt::print(shortcut_stream, "Comment={}\n", comment);
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        fmt::print(shortcut_stream, "Icon={}\n", icon_path.string());
    }
    fmt::print(shortcut_stream, "TryExec={}\n", command.string());
    fmt::print(shortcut_stream, "Exec={} {}\n", command.string(), arguments);
    if (!categories.empty()) {
        fmt::print(shortcut_stream, "Categories={}\n", categories);
    }
    if (!keywords.empty()) {
        fmt::print(shortcut_stream, "Keywords={}\n", keywords);
    }
    return true;
#elif defined(_WIN32) // Windows
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) {
        LOG_ERROR(Frontend, "CoInitialize failed");
        return false;
    }
    SCOPE_EXIT {
        CoUninitialize();
    };
    IShellLinkW* ps1 = nullptr;
    IPersistFile* persist_file = nullptr;
    SCOPE_EXIT {
        if (persist_file != nullptr) {
            persist_file->Release();
        }
        if (ps1 != nullptr) {
            ps1->Release();
        }
    };
    HRESULT hres = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                    reinterpret_cast<void**>(&ps1));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to create IShellLinkW instance");
        return false;
    }
    hres = ps1->SetPath(command.c_str());
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to set path");
        return false;
    }
    if (!arguments.empty()) {
        hres = ps1->SetArguments(Common::UTF8ToUTF16W(arguments).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set arguments");
            return false;
        }
    }
    if (!comment.empty()) {
        hres = ps1->SetDescription(Common::UTF8ToUTF16W(comment).data());
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set description");
            return false;
        }
    }
    if (std::filesystem::is_regular_file(icon_path)) {
        hres = ps1->SetIconLocation(icon_path.c_str(), 0);
        if (FAILED(hres)) {
            LOG_ERROR(Frontend, "Failed to set icon location");
            return false;
        }
    }
    hres = ps1->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&persist_file));
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to get IPersistFile interface");
        return false;
    }
    hres = persist_file->Save(std::filesystem::path{shortcut_path / (name + ".lnk")}.c_str(), TRUE);
    if (FAILED(hres)) {
        LOG_ERROR(Frontend, "Failed to save shortcut");
        return false;
    }
    return true;
#else                 // Unsupported platform
    return false;
#endif
} catch (const std::exception& e) {
    LOG_ERROR(Frontend, "Failed to create shortcut: {}", e.what());
    return false;
}
// Messages in pre-defined message boxes for less code spaghetti
bool GMainWindow::CreateShortcutMessagesGUI(QWidget* parent, int imsg, const QString& game_title) {
    int result = 0;
    QMessageBox::StandardButtons buttons;
    switch (imsg) {
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_FULLSCREEN_YES:
        buttons = QMessageBox::Yes | QMessageBox::No;
        result =
            QMessageBox::information(parent, tr("Create Shortcut"),
                                     tr("Do you want to launch the game in fullscreen?"), buttons);
        return result == QMessageBox::Yes;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_SUCCESS:
        QMessageBox::information(parent, tr("Create Shortcut"),
                                 tr("Successfully created a shortcut to %1").arg(game_title));
        return false;
    case GMainWindow::CREATE_SHORTCUT_MSGBOX_APPVOLATILE_WARNING:
        buttons = QMessageBox::StandardButton::Ok | QMessageBox::StandardButton::Cancel;
        result =
            QMessageBox::warning(this, tr("Create Shortcut"),
                                 tr("This will create a shortcut to the current AppImage. This may "
                                    "not work well if you update. Continue?"),
                                 buttons);
        return result == QMessageBox::Ok;
    default:
        buttons = QMessageBox::Ok;
        QMessageBox::critical(parent, tr("Create Shortcut"),
                              tr("Failed to create a shortcut to %1").arg(game_title), buttons);
        return false;
    }
}

bool GMainWindow::MakeShortcutIcoPath(const u64 program_id, const std::string_view game_file_name,
                                      std::filesystem::path& out_icon_path) {
    // Get path to Suyu icons directory & icon extension
    std::string ico_extension = "png";
#if defined(_WIN32)
    out_icon_path = Common::FS::GetSuyuPath(Common::FS::SuyuPath::IconsDir);
    ico_extension = "ico";
#elif defined(__linux__) || defined(__FreeBSD__)
    out_icon_path = Common::FS::GetDataDirectory("XDG_DATA_HOME") / "icons/hicolor/256x256";
#endif
    // Create icons directory if it doesn't exist
    if (!Common::FS::CreateDirs(out_icon_path)) {
        QMessageBox::critical(
            this, tr("Create Icon"),
            tr("Cannot create icon file. Path \"%1\" does not exist and cannot be created.")
                .arg(QString::fromStdString(out_icon_path.string())),
            QMessageBox::StandardButton::Ok);
        out_icon_path.clear();
        return false;
    }

    // Create icon file path
    out_icon_path /= (program_id == 0 ? fmt::format("suyu-{}.{}", game_file_name, ico_extension)
                                      : fmt::format("suyu-{:016X}.{}", program_id, ico_extension));
    return true;
}

void GMainWindow::OnGameListCreateShortcut(u64 program_id, const std::string& game_path,
                                           GameListShortcutTarget target) {
    // Get path to suyu executable
    const QStringList args = QApplication::arguments();
    std::filesystem::path suyu_command = args[0].toStdString();
    // If relative path, make it an absolute path
    if (suyu_command.c_str()[0] == '.') {
        suyu_command = Common::FS::GetCurrentDir() / suyu_command;
    }
    // Shortcut path
    std::filesystem::path shortcut_path{};
    if (target == GameListShortcutTarget::Desktop) {
        shortcut_path =
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation).toStdString();
    } else if (target == GameListShortcutTarget::Applications) {
        shortcut_path =
            QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation).toStdString();
    }

    if (!std::filesystem::exists(shortcut_path)) {
        GMainWindow::CreateShortcutMessagesGUI(
            this, GMainWindow::CREATE_SHORTCUT_MSGBOX_ERROR,
            QString::fromStdString(shortcut_path.generic_string()));
        LOG_ERROR(Frontend, "Invalid shortcut target {}", shortcut_path.generic_string());
        return;
    }

    // Get title from game file
    const FileSys::PatchManager pm{program_id, system->GetFileSystemController(),
                                   system->GetContentProvider()};
    const auto control = pm.GetControlMetadata();
    const auto loader =
        Loader::GetLoader(*system, vfs->OpenFile(game_path, FileSys::OpenMode::Read));
    std::string game_title = fmt::format("{:016X}", program_id);
    if (control.first != nullptr) {
        game_title = control.first->GetApplicationName();
    } else {
        loader->ReadTitle(game_title);
    }
    // Delete illegal characters from title
    const std::string illegal_chars = "<>:\"/\\|?*.";
    for (auto it = game_title.rbegin(); it != game_title.rend(); ++it) {
        if (illegal_chars.find(*it) != std::string::npos) {
            game_title.erase(it.base() - 1);
        }
    }
    const QString qt_game_title = QString::fromStdString(game_title);
    // Get icon from game file
    std::vector<u8> icon_image_file{};
    if (control.second != nullptr) {
        icon_image_file = control.second->ReadAllBytes();
    } else if (loader->ReadIcon(icon_image_file) != Loader::ResultStatus::Success) {
        LOG_WARNING(Frontend, "Could not read icon from {:s}", game_path);
    }
    QImage icon_data =
        QImage::fromData(icon_image_file.data(), static_cast<int>(icon_image_file.size()));
    std::filesystem::path out_icon_path;
    if (GMainWindow::MakeShortcutIcoPath(program_id, game_title, out_icon_path)) {
        if (!SaveIconToFile(out_icon_path, icon_data)) {
            LOG_ERROR(Frontend, "Could not write icon to file");
        }
    }

#if defined(__linux__)
    // Special case for AppImages
    // Warn once if we are making a shortcut to a volatile AppImage
    const std::string appimage_ending =
        std::string(Common::g_scm_rev).substr(0, 9).append(".AppImage");
    if (suyu_command.string().ends_with(appimage_ending) &&
        !UISettings::values.shortcut_already_warned) {
        if (GMainWindow::CreateShortcutMessagesGUI(
                this, GMainWindow::CREATE_SHORTCUT_MSGBOX_APPVOLATILE_WARNING, qt_game_title)) {
            return;
        }
        UISettings::values.shortcut_already_warned = true;
    }
#endif // __linux__
    // Create shortcut
    std::string arguments = fmt::format("-g \"{:s}\"", game_path);
    if (GMainWindow::CreateShortcutMessagesGUI(
            this, GMainWindow::CREATE_SHORTCUT_MSGBOX_FULLSCREEN_YES, qt_game_title)) {
        arguments = "-f " + arguments;
    }
    const std::string comment = fmt::format("Start {:s} with the suyu Emulator", game_title);
    const std::string categories = "Game;Emulator;Qt;";
    const std::string keywords = "Switch;Nintendo;";

    if (GMainWindow::CreateShortcutLink(shortcut_path, comment, out_icon_path, suyu_command,
                                        arguments, categories, keywords, game_title)) {
        GMainWindow::CreateShortcutMessagesGUI(this, GMainWindow::CREATE_SHORTCUT_MSGBOX_SUCCESS,
                                               qt_game_title);
        return;
    }
    GMainWindow::CreateShortcutMessagesGUI(this, GMainWindow::CREATE_SHORTCUT_MSGBOX_ERROR,
                                           qt_game_title);
}

void GMainWindow::OnGameListOpenDirectory(const QString& directory) {
    std::filesystem::path fs_path;
    if (directory == QStringLiteral("SDMC")) {
        fs_path =
            Common::FS::GetSuyuPath(Common::FS::SuyuPath::SDMCDir) / "Nintendo/Contents/registered";
    } else if (directory == QStringLiteral("UserNAND")) {
        fs_path =
            Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir) / "user/Contents/registered";
    } else if (directory == QStringLiteral("SysNAND")) {
        fs_path =
            Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir) / "system/Contents/registered";
    } else {
        fs_path = directory.toStdString();
    }

    const auto qt_path = QString::fromStdString(Common::FS::PathToUTF8String(fs_path));

    if (!Common::FS::IsDir(fs_path)) {
        QMessageBox::critical(this, tr("Error Opening %1").arg(qt_path),
                              tr("Folder does not exist!"));
        return;
    }

    QDesktopServices::openUrl(QUrl::fromLocalFile(qt_path));
}

void GMainWindow::OnGameListAddDirectory() {
    const QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (dir_path.isEmpty()) {
        return;
    }

    // Newly added folders should scan subdirectories by default so users can add
    // a game collection root without manually toggling "Scan Subfolders".
    UISettings::GameDir game_dir{dir_path.toStdString(), true, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        game_list->PopulateAsync(UISettings::values.game_dirs);
    } else {
        LOG_WARNING(Frontend, "Selected directory is already in the game list");
    }

    OnSaveConfig();
}

void GMainWindow::OnGameListShowList(bool show) {
    if (current_mode_ == AppMode::Gamer || current_mode_ == AppMode::Programmer ||
        (gamer_env_ && gamer_env_->isVisible()) ||
        (programmer_env_ && programmer_env_->isVisible())) {
        game_list->setVisible(false);
        game_list_placeholder->setVisible(false);
        return;
    }

    if (emulation_running && ui->action_Single_Window_Mode->isChecked())
        return;

    if (show && !UISettings::values.game_dirs.isEmpty() && game_list->IsEmpty()) {
        game_list->LoadCompatibilityList();
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    game_list->setVisible(show);
    game_list_placeholder->setVisible(!show);
};

void GMainWindow::OnGameListOpenPerGameProperties(const std::string& file) {
    u64 title_id{};
    const auto v_file = Core::GetGameFileFromPath(vfs, file);
    const auto loader = Loader::GetLoader(*system, v_file);

    if (loader == nullptr || loader->ReadProgramId(title_id) != Loader::ResultStatus::Success) {
        QMessageBox::information(this, tr("Properties"),
                                 tr("The game properties could not be loaded."));
        return;
    }

    OpenPerGameConfiguration(title_id, file);
}

void GMainWindow::OnMenuLoadFile() {
    if (is_load_file_select_active) {
        return;
    }

    is_load_file_select_active = true;
    const QString extensions =
        QStringLiteral("*.")
            .append(GameList::supported_file_extensions.join(QStringLiteral(" *.")))
            .append(QStringLiteral(" main"));
    const QString file_filter = tr("Switch Executable (%1);;All Files (*.*)",
                                   "%1 is an identifier for the Switch executable file extensions.")
                                    .arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load File"), QString::fromStdString(UISettings::values.roms_path), file_filter);
    is_load_file_select_active = false;

    if (filename.isEmpty()) {
        return;
    }

    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename, ApplicationAppletParameters());
}

void GMainWindow::OnMenuLoadFolder() {
    const QString dir_path =
        QFileDialog::getExistingDirectory(this, tr("Open Extracted ROM Directory"));

    if (dir_path.isNull()) {
        return;
    }

    const QDir dir{dir_path};
    const QStringList matching_main = dir.entryList({QStringLiteral("main")}, QDir::Files);
    if (matching_main.size() == 1) {
        BootGame(dir.path() + QDir::separator() + matching_main[0], ApplicationAppletParameters());
    } else {
        QMessageBox::warning(this, tr("Invalid Directory Selected"),
                             tr("The directory you have selected does not contain a 'main' file."));
    }
}

void GMainWindow::IncrementInstallProgress() {
    install_progress->setValue(install_progress->value() + 1);
}

void GMainWindow::OnMenuInstallToNAND() {
    const QString file_filter =
        tr("Installable Switch File (*.nca *.nsp *.xci);;Nintendo Content Archive "
           "(*.nca);;Nintendo Submission Package (*.nsp);;NX Cartridge "
           "Image (*.xci)");

    QStringList filenames = QFileDialog::getOpenFileNames(
        this, tr("Install Files"), QString::fromStdString(UISettings::values.roms_path),
        file_filter);

    if (filenames.isEmpty()) {
        return;
    }

    InstallDialog installDialog(this, filenames);
    if (installDialog.exec() == QDialog::Rejected) {
        return;
    }

    const QStringList files = installDialog.GetFiles();

    if (files.isEmpty()) {
        return;
    }

    // Save folder location of the first selected file
    UISettings::values.roms_path = QFileInfo(filenames[0]).path().toStdString();

    int remaining = filenames.size();

    // This would only overflow above 2^51 bytes (2.252 PB)
    int total_size = 0;
    for (const QString& file : files) {
        total_size += static_cast<int>(QFile(file).size() / CopyBufferSize);
    }
    if (total_size < 0) {
        LOG_CRITICAL(Frontend, "Attempting to install too many files, aborting.");
        return;
    }

    QStringList new_files{};         // Newly installed files that do not yet exist in the NAND
    QStringList overwritten_files{}; // Files that overwrote those existing in the NAND
    QStringList failed_files{};      // Files that failed to install due to errors
    bool detected_base_install{};    // Whether a base game was attempted to be installed

    ui->action_Install_File_NAND->setEnabled(false);

    install_progress = new QProgressDialog(QString{}, tr("Cancel"), 0, total_size, this);
    install_progress->setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    install_progress->setAttribute(Qt::WA_DeleteOnClose, true);
    install_progress->setFixedWidth(installDialog.GetMinimumWidth() + 40);
    install_progress->show();

    for (const QString& file : files) {
        install_progress->setWindowTitle(tr("%n file(s) remaining", "", remaining));
        install_progress->setLabelText(
            tr("Installing file \"%1\"...").arg(QFileInfo(file).fileName()));

        QFuture<ContentManager::InstallResult> future;
        ContentManager::InstallResult result;

        if (file.endsWith(QStringLiteral("nsp"), Qt::CaseInsensitive)) {
            const auto progress_callback = [this](size_t size, size_t progress) {
                emit UpdateInstallProgress();
                if (install_progress->wasCanceled()) {
                    return true;
                }
                return false;
            };
            future = QtConcurrent::run([this, &file, progress_callback] {
                return ContentManager::InstallNSP(*system, *vfs, file.toStdString(),
                                                  progress_callback);
            });

            while (!future.isFinished()) {
                QCoreApplication::processEvents();
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }

            result = future.result();

        } else {
            result = InstallNCA(file);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        switch (result) {
        case ContentManager::InstallResult::Success:
            new_files.append(QFileInfo(file).fileName());
            break;
        case ContentManager::InstallResult::Overwrite:
            overwritten_files.append(QFileInfo(file).fileName());
            break;
        case ContentManager::InstallResult::Failure:
            failed_files.append(QFileInfo(file).fileName());
            break;
        case ContentManager::InstallResult::BaseInstallAttempted:
            failed_files.append(QFileInfo(file).fileName());
            detected_base_install = true;
            break;
        }

        --remaining;
    }

    install_progress->close();

    if (detected_base_install) {
        QMessageBox::warning(
            this, tr("Install Results"),
            tr("To avoid possible conflicts, we discourage users from installing base games to the "
               "NAND.\nPlease, only use this feature to install updates and DLC."));
    }

    const QString install_results =
        (new_files.isEmpty() ? QString{}
                             : tr("%n file(s) were newly installed\n", "", new_files.size())) +
        (overwritten_files.isEmpty()
             ? QString{}
             : tr("%n file(s) were overwritten\n", "", overwritten_files.size())) +
        (failed_files.isEmpty() ? QString{}
                                : tr("%n file(s) failed to install\n", "", failed_files.size()));

    QMessageBox::information(this, tr("Install Results"), install_results);
    Common::FS::RemoveDirRecursively(Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir) /
                                     "game_list");
    game_list->PopulateAsync(UISettings::values.game_dirs);
    ui->action_Install_File_NAND->setEnabled(true);
}

ContentManager::InstallResult GMainWindow::InstallNCA(const QString& filename) {
    const QStringList tt_options{tr("System Application"),
                                 tr("System Archive"),
                                 tr("System Application Update"),
                                 tr("Firmware Package (Type A)"),
                                 tr("Firmware Package (Type B)"),
                                 tr("Game"),
                                 tr("Game Update"),
                                 tr("Game DLC"),
                                 tr("Delta Title")};
    bool ok;
    const auto item = QInputDialog::getItem(
        this, tr("Select NCA Install Type..."),
        tr("Please select the type of title you would like to install this NCA as:\n(In "
           "most instances, the default 'Game' is fine.)"),
        tt_options, 5, false, &ok);

    auto index = tt_options.indexOf(item);
    if (!ok || index == -1) {
        QMessageBox::warning(this, tr("Failed to Install"),
                             tr("The title type you selected for the NCA is invalid."));
        return ContentManager::InstallResult::Failure;
    }

    // If index is equal to or past Game, add the jump in TitleType.
    if (index >= 5) {
        index += static_cast<size_t>(FileSys::TitleType::Application) -
                 static_cast<size_t>(FileSys::TitleType::FirmwarePackageB);
    }

    const bool is_application = index >= static_cast<s32>(FileSys::TitleType::Application);
    const auto& fs_controller = system->GetFileSystemController();
    auto* registered_cache = is_application ? fs_controller.GetUserNANDContents()
                                            : fs_controller.GetSystemNANDContents();

    const auto progress_callback = [this](size_t size, size_t progress) {
        emit UpdateInstallProgress();
        if (install_progress->wasCanceled()) {
            return true;
        }
        return false;
    };
    return ContentManager::InstallNCA(*vfs, filename.toStdString(), *registered_cache,
                                      static_cast<FileSys::TitleType>(index), progress_callback);
}

void GMainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    assert(action);

    const QString filename = action->data().toString();
    if (QFileInfo::exists(filename)) {
        BootGame(filename, ApplicationAppletParameters());
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"),
                                 tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void GMainWindow::OnStartGame() {
    PreventOSSleep();

    emu_thread->SetRunning(true);

    UpdateMenuState();
    OnTasStateChanged();

    play_time_manager->SetProgramId(system->GetApplicationProcessProgramID());
    play_time_manager->Start();

    discord_rpc->Update();

#ifdef __unix__
    Common::Linux::StartGamemode();
#endif
}

void GMainWindow::OnRestartGame() {
    if (!system->IsPoweredOn()) {
        return;
    }

    if (ConfirmShutdownGame()) {
        // Make a copy since ShutdownGame edits game_path
        const auto current_game = QString(current_game_path);
        ShutdownGame();
        BootGame(current_game, ApplicationAppletParameters());
    }
}

void GMainWindow::OnPauseGame() {
    emu_thread->SetRunning(false);
    play_time_manager->Stop();
    UpdateMenuState();
    AllowOSSleep();

#ifdef __unix__
    Common::Linux::StopGamemode();
#endif
}

void GMainWindow::OnPauseContinueGame() {
    if (emulation_running) {
        if (emu_thread->IsRunning()) {
            OnPauseGame();
        } else {
            OnStartGame();
        }
    }
}

void GMainWindow::OnStopGame() {
    if (ConfirmShutdownGame()) {
        play_time_manager->Stop();
        // Update game list to show new play time
        game_list->PopulateAsync(UISettings::values.game_dirs);
        if (OnShutdownBegin()) {
            OnShutdownBeginDialog();
        } else {
            OnEmulationStopped();
        }
    }
}

bool GMainWindow::ConfirmShutdownGame() {
    if (UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Always) {
        if (system->GetExitLocked()) {
            if (!ConfirmForceLockedExit()) {
                return false;
            }
        } else {
            if (!ConfirmChangeGame()) {
                return false;
            }
        }
    } else {
        if (UISettings::values.confirm_before_stopping.GetValue() ==
                ConfirmStop::Ask_Based_On_Game &&
            system->GetExitLocked()) {
            if (!ConfirmForceLockedExit()) {
                return false;
            }
        }
    }
    return true;
}

void GMainWindow::OnLoadComplete() {
    loading_screen->OnLoadComplete();
}

void GMainWindow::OnExecuteProgram(std::size_t program_index) {
    ShutdownGame();

    auto params = ApplicationAppletParameters();
    params.program_index = static_cast<s32>(program_index);
    params.launch_type = Service::AM::LaunchType::ApplicationInitiated;
    BootGame(last_filename_booted, params);
}

void GMainWindow::OnExit() {
    ShutdownGame();
}

void GMainWindow::OnSaveConfig() {
    system->ApplySettings();
    config->SaveAllValues();
}

void GMainWindow::ErrorDisplayDisplayError(QString error_code, QString error_text) {
    error_applet = new OverlayDialog(render_window, *system, error_code, error_text, QString{},
                                     tr("OK"), Qt::AlignLeft | Qt::AlignVCenter);
    SCOPE_EXIT {
        error_applet->deleteLater();
        error_applet = nullptr;
    };
    error_applet->exec();

    emit ErrorDisplayFinished();
}

void GMainWindow::ErrorDisplayRequestExit() {
    if (error_applet) {
        error_applet->reject();
    }
}

void GMainWindow::OnMenuReportCompatibility() {
#if defined(ARCHITECTURE_x86_64) && !defined(__APPLE__)
    const auto& caps = Common::g_cpu_caps;
    const bool has_fma = caps.fma;
    const auto processor_count = std::thread::hardware_concurrency();
    const bool has_4threads = processor_count == 0 || processor_count >= 4;
    const bool has_8gb_ram = Common::GetMemInfo().TotalPhysicalMemory >= 8_GiB;
    const bool has_broken_vulkan = UISettings::values.has_broken_vulkan;

    if (!has_fma || !has_4threads || !has_8gb_ram || has_broken_vulkan) {
        QMessageBox::critical(this, tr("Hardware requirements not met"),
                              tr("Your system does not meet the recommended hardware requirements. "
                                 "Compatibility reporting has been disabled."));
        return;
    }

    if (!Settings::values.eden_token.GetValue().empty() &&
        !Settings::values.eden_username.GetValue().empty()) {
        CompatDB compatdb{this};
        compatdb.exec();
    } else {
        QMessageBox::critical(
            this, tr("Missing Eden Account"),
            tr("In order to submit a game compatibility test case, you must link your Eden "
               "account.<br><br/>To link your Eden account, go to Emulation &gt; Configuration "
               "&gt; "
               "Web."));
    }
#else
    QMessageBox::critical(this, tr("Hardware requirements not met"),
                          tr("Your system does not meet the recommended hardware requirements. "
                             "Compatibility reporting has been disabled."));
#endif
}

void GMainWindow::OpenURL(const QUrl& url) {
    const bool open = QDesktopServices::openUrl(url);
    if (!open) {
        QMessageBox::warning(this, tr("Error opening URL"),
                             tr("Unable to open the requested URL."));
    }
}

void GMainWindow::OnOpenModsPage() {
    OpenURL(QUrl(QStringLiteral("https://suyu-emu.github.io/website/")));
}

void GMainWindow::OnOpenQuickstartGuide() {
    OpenURL(QUrl(QStringLiteral("https://suyu-emu.github.io/website/")));
}

void GMainWindow::OnOpenFAQ() {
    OpenURL(QUrl(QStringLiteral("https://suyu-emu.github.io/website/")));
}

bool GMainWindow::UsingExclusiveFullscreen() {
    return Settings::values.fullscreen_mode.GetValue() == Settings::FullscreenMode::Exclusive;
}

void GMainWindow::ToggleFullscreen() {
    if (ui->action_Fullscreen->isChecked()) {
        ShowFullscreen();
    } else {
        HideFullscreen();
    }
}

void GMainWindow::ShowFullscreen() {
    const auto show_fullscreen = [this](QWidget* window) {
        if (UsingExclusiveFullscreen()) {
            window->showFullScreen();
        } else {
            window->hide();
            window->setWindowFlags(window->windowFlags() | Qt::FramelessWindowHint);
            window->raise();
            window->show();
        }
    };

    if (ui->action_Single_Window_Mode->isChecked()) {
        UISettings::values.geometry = saveGeometry();

        ui->menubar->hide();
        statusBar()->hide();

        show_fullscreen(this);
    } else {
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
        show_fullscreen(render_window);
    }
}

void GMainWindow::HideFullscreen() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        if (UsingExclusiveFullscreen()) {
            showNormal();
            restoreGeometry(UISettings::values.geometry);
        } else {
            hide();
            setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
            restoreGeometry(UISettings::values.geometry);
            raise();
            show();
        }

        statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
        ui->menubar->show();
    } else {
        if (UsingExclusiveFullscreen()) {
            render_window->showNormal();
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
        } else {
            render_window->hide();
            render_window->setWindowFlags(windowFlags() & ~Qt::FramelessWindowHint);
            render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
            render_window->raise();
            render_window->show();
        }
    }
}

void GMainWindow::ToggleWindowMode() {
    if (ui->action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        render_window->BackupGeometry();
        ui->emulationLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::StrongFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            game_list->hide();
        }

    } else {
        // Render in a separate window...
        ui->emulationLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        render_window->setFocusPolicy(Qt::NoFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->RestoreGeometry();
            if (current_mode_ == AppMode::Hacker) {
                game_list->show();
            } else if (current_mode_ == AppMode::Gamer && gamer_env_) {
                gamer_env_->show();
            } else if (current_mode_ == AppMode::Programmer && programmer_env_) {
                programmer_env_->show();
            }
        }
    }
}

void GMainWindow::ResetWindowSize(u32 width, u32 height) {
    const auto aspect_ratio = Layout::EmulationAspectRatio(
        static_cast<Settings::AspectRatio>(Settings::values.aspect_ratio.GetValue()),
        static_cast<float>(height) / width);
    if (!ui->action_Single_Window_Mode->isChecked()) {
        render_window->resize(height / aspect_ratio, height);
    } else {
        const bool show_status_bar = ui->action_Show_Status_Bar->isChecked();
        const auto status_bar_height = show_status_bar ? statusBar()->height() : 0;
        resize(height / aspect_ratio, height + menuBar()->height() + status_bar_height);
    }
}

void GMainWindow::ResetWindowSize720() {
    ResetWindowSize(Layout::ScreenUndocked::Width, Layout::ScreenUndocked::Height);
}

void GMainWindow::ResetWindowSize800() {
    ResetWindowSize(1280U, 800U);
}

void GMainWindow::ResetWindowSize900() {
    ResetWindowSize(1600U, 900U);
}

void GMainWindow::ResetWindowSize1080() {
    ResetWindowSize(Layout::ScreenDocked::Width, Layout::ScreenDocked::Height);
}

void GMainWindow::OnConfigure() {
    const QString old_theme = UISettings::values.theme;
    DarkModeState old_dark_mode_state = UISettings::values.dark_mode_state;
    const bool old_discord_presence = UISettings::values.enable_discord_presence.GetValue();
    const auto old_language_index = Settings::values.language_index.GetValue();
#ifdef __unix__
    const bool old_gamemode = Settings::values.enable_gamemode.GetValue();
#endif

    Settings::SetConfiguringGlobal(true);
    ConfigureDialog configure_dialog(this, hotkey_registry, input_subsystem.get(),
                                     vk_device_records, *system,
                                     !multiplayer_state->IsHostingPublicRoom());
    connect(&configure_dialog, &ConfigureDialog::LanguageChanged, this,
            &GMainWindow::OnLanguageChanged);

    const auto result = configure_dialog.exec();
    if (result != QDialog::Accepted && !UISettings::values.configuration_applied &&
        !UISettings::values.reset_to_defaults) {
        // Runs if the user hit Cancel or closed the window, and did not ever press the Apply button
        // or `Reset to Defaults` button
        return;
    } else if (result == QDialog::Accepted) {
        // Only apply new changes if user hit Okay
        // This is here to avoid applying changes if the user hit Apply, made some changes, then hit
        // Cancel
        try {
            configure_dialog.ApplyConfiguration();
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "Exception applying configuration: {}", e.what());
            QMessageBox::critical(this, tr("Settings Error"),
                                  tr("Failed to apply settings:\n%1")
                                      .arg(QString::fromUtf8(e.what())));
        } catch (...) {
            LOG_ERROR(Frontend, "Unknown exception applying configuration");
            QMessageBox::critical(this, tr("Settings Error"),
                                  tr("An unexpected error occurred while applying settings."));
        }
    } else if (UISettings::values.reset_to_defaults) {
        LOG_INFO(Frontend, "Resetting all settings to defaults");
        if (!Common::FS::RemoveFile(config->GetConfigFilePath())) {
            LOG_WARNING(Frontend, "Failed to remove configuration file");
        }
        if (!Common::FS::RemoveDirContentsRecursively(
                Common::FS::GetSuyuPath(Common::FS::SuyuPath::ConfigDir) / "custom")) {
            LOG_WARNING(Frontend, "Failed to remove custom configuration files");
        }
        if (!Common::FS::RemoveDirRecursively(
                Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir) / "game_list")) {
            LOG_WARNING(Frontend, "Failed to remove game metadata cache files");
        }

        // Explicitly save the game directories, since reinitializing config does not explicitly do
        // so.
        QVector<UISettings::GameDir> old_game_dirs = std::move(UISettings::values.game_dirs);
        QVector<u64> old_favorited_ids = std::move(UISettings::values.favorited_ids);

        Settings::values.disabled_addons.clear();

        try {
            config = std::make_unique<QtConfig>();
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "Exception reinitializing config: {}", e.what());
            QMessageBox::critical(this, tr("Settings Error"),
                                  tr("Failed to reset configuration:\n%1")
                                      .arg(QString::fromUtf8(e.what())));
        } catch (...) {
            LOG_ERROR(Frontend, "Unknown exception reinitializing config");
            QMessageBox::critical(this, tr("Settings Error"),
                                  tr("An unexpected error occurred while resetting configuration."));
        }
        UISettings::values.reset_to_defaults = false;

        UISettings::values.game_dirs = std::move(old_game_dirs);
        UISettings::values.favorited_ids = std::move(old_favorited_ids);

        InitializeRecentFileMenuActions();

        SetDefaultUIGeometry();
        RestoreUIState();
    }
    InitializeHotkeys();

    if (UISettings::values.theme != old_theme ||
        UISettings::values.dark_mode_state != old_dark_mode_state) {
        UpdateUITheme();
    }
    if (UISettings::values.enable_discord_presence.GetValue() != old_discord_presence) {
        SetDiscordEnabled(UISettings::values.enable_discord_presence.GetValue());
    }
#ifdef __unix__
    if (Settings::values.enable_gamemode.GetValue() != old_gamemode) {
        SetGamemodeEnabled(Settings::values.enable_gamemode.GetValue());
    }
#endif

    if (multiplayer_state && !multiplayer_state->IsHostingPublicRoom()) {
        try {
            multiplayer_state->UpdateCredentials();
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "Exception updating multiplayer credentials: {}", e.what());
        } catch (...) {
            LOG_ERROR(Frontend, "Unknown exception updating multiplayer credentials");
        }
    }

    const auto reload = UISettings::values.is_game_list_reload_pending.exchange(false);
    if (reload || Settings::values.language_index.GetValue() != old_language_index) {
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    UISettings::values.configuration_applied = false;

    try {
        config->SaveAllValues();
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception saving config: {}", e.what());
    } catch (...) {
        LOG_ERROR(Frontend, "Unknown exception saving config");
    }

    if ((UISettings::values.hide_mouse || Settings::values.mouse_panning) && emulation_running) {
        render_window->installEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, true);
    } else {
        render_window->removeEventFilter(render_window);
        render_window->setAttribute(Qt::WA_Hover, false);
    }

    if (UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }

    // Restart camera config
    if (emulation_running) {
        render_window->FinalizeCamera();
        render_window->InitializeCamera();
    }

    if (!UISettings::values.has_broken_vulkan) {
        renderer_status_button->setEnabled(!emulation_running);
    }

    UpdateStatusButtons();
    controller_dialog->refreshConfiguration();
    system->ApplySettings();
}

void GMainWindow::OnChangeInterfaceMode() {
    ModeSelector selector(this);
    if (selector.exec() == QDialog::Accepted) {
        ApplyAppMode(selector.SelectedMode());
    }
}

void GMainWindow::OnConfigureTas() {
    ConfigureTasDialog dialog(this);
    const auto result = dialog.exec();

    if (result != QDialog::Accepted && !UISettings::values.configuration_applied) {
        Settings::RestoreGlobalState(system->IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
        OnSaveConfig();
    }
}

void GMainWindow::OnTasStartStop() {
    if (!emulation_running) {
        return;
    }

    // Disable system buttons to prevent TAS from executing a hotkey
    auto* controller = system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    controller->ResetSystemButtons();

    input_subsystem->GetTas()->StartStop();
    OnTasStateChanged();
}

void GMainWindow::OnTasRecord() {
    if (!emulation_running) {
        return;
    }
    if (is_tas_recording_dialog_active) {
        return;
    }

    // Disable system buttons to prevent TAS from recording a hotkey
    auto* controller = system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    controller->ResetSystemButtons();

    const bool is_recording = input_subsystem->GetTas()->Record();
    if (!is_recording) {
        is_tas_recording_dialog_active = true;

        bool answer = question(this, tr("TAS Recording"), tr("Overwrite file of player 1?"),
                               QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

        input_subsystem->GetTas()->SaveRecording(answer);
        is_tas_recording_dialog_active = false;
    }
    OnTasStateChanged();
}

void GMainWindow::OnTasReset() {
    input_subsystem->GetTas()->Reset();
}

void GMainWindow::OnToggleDockedMode() {
    const bool is_docked = Settings::IsDockedMode();
    auto* player_1 = system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Player1);
    auto* handheld = system->HIDCore().GetEmulatedController(Core::HID::NpadIdType::Handheld);

    if (!is_docked && handheld->IsConnected()) {
        QMessageBox::warning(this, tr("Invalid config detected"),
                             tr("Handheld controller can't be used on docked mode. Pro "
                                "controller will be selected."));
        handheld->Disconnect();
        player_1->SetNpadStyleIndex(Core::HID::NpadStyleIndex::Fullkey);
        player_1->Connect();
        controller_dialog->refreshConfiguration();
    }

    Settings::values.use_docked_mode.SetValue(is_docked ? Settings::ConsoleMode::Handheld
                                                        : Settings::ConsoleMode::Docked);
    UpdateDockedButton();
    OnDockedModeChanged(is_docked, !is_docked, *system);
}

void GMainWindow::OnToggleGpuAccuracy() {
    switch (Settings::values.gpu_accuracy.GetValue()) {
    case Settings::GpuAccuracy::High: {
        Settings::values.gpu_accuracy.SetValue(Settings::GpuAccuracy::Low);
        break;
    }
    case Settings::GpuAccuracy::Low:
    default: {
        Settings::values.gpu_accuracy.SetValue(Settings::GpuAccuracy::High);
        break;
    }
    }

    system->ApplySettings();
    UpdateGPUAccuracyButton();
}

void GMainWindow::OnMute() {
    Settings::values.audio_muted = !Settings::values.audio_muted;
    UpdateVolumeUI();
}

void GMainWindow::OnDecreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume = static_cast<s32>(Settings::values.volume.GetValue());
    int step = 5;
    if (current_volume <= 30) {
        step = 2;
    }
    if (current_volume <= 6) {
        step = 1;
    }
    Settings::values.volume.SetValue(std::max(current_volume - step, 0));
    UpdateVolumeUI();
}

void GMainWindow::OnIncreaseVolume() {
    Settings::values.audio_muted = false;
    const auto current_volume = static_cast<s32>(Settings::values.volume.GetValue());
    int step = 5;
    if (current_volume < 30) {
        step = 2;
    }
    if (current_volume < 6) {
        step = 1;
    }
    Settings::values.volume.SetValue(current_volume + step);
    UpdateVolumeUI();
}

void GMainWindow::OnToggleAdaptingFilter() {
    auto filter = Settings::values.scaling_filter.GetValue();
    filter = static_cast<Settings::ScalingFilter>(static_cast<u32>(filter) + 1);
    if (static_cast<u32>(filter) > static_cast<u32>(Settings::EnumMetadata<Settings::ScalingFilter>::GetLast())) {
        filter = Settings::ScalingFilter::NearestNeighbor;
    }
    Settings::values.scaling_filter.SetValue(filter);
    filter_status_button->setChecked(true);
    UpdateFilterText();
}

void GMainWindow::OnToggleGraphicsAPI() {
    auto api = Settings::values.renderer_backend.GetValue();
    if (api != Settings::RendererBackend::Vulkan) {
        api = Settings::RendererBackend::Vulkan;
    } else {
#ifdef HAS_OPENGL
        api = Settings::RendererBackend::OpenGL_SPIRV;
#else
        api = Settings::RendererBackend::Null;
#endif
    }
    Settings::values.renderer_backend.SetValue(api);
    renderer_status_button->setChecked(api == Settings::RendererBackend::Vulkan);
    UpdateAPIText();
}

void GMainWindow::OnConfigurePerGame() {
    const u64 title_id = system->GetApplicationProcessProgramID();
    OpenPerGameConfiguration(title_id, current_game_path.toStdString());
}

void GMainWindow::OpenPerGameConfiguration(u64 title_id, const std::string& file_name) {
    const auto v_file = Core::GetGameFileFromPath(vfs, file_name);

    Settings::SetConfiguringGlobal(false);
    ConfigurePerGame dialog(this, title_id, file_name, vk_device_records, *system);
    dialog.LoadFromFile(v_file);
    const auto result = dialog.exec();

    if (result != QDialog::Accepted && !UISettings::values.configuration_applied) {
        Settings::RestoreGlobalState(system->IsPoweredOn());
        return;
    } else if (result == QDialog::Accepted) {
        dialog.ApplyConfiguration();
    }

    const auto reload = UISettings::values.is_game_list_reload_pending.exchange(false);
    if (reload) {
        game_list->PopulateAsync(UISettings::values.game_dirs);
    }

    // Do not cause the global config to write local settings into the config file
    const bool is_powered_on = system->IsPoweredOn();
    Settings::RestoreGlobalState(is_powered_on);
    system->HIDCore().ReloadInputDevices();

    UISettings::values.configuration_applied = false;

    if (!is_powered_on) {
        config->SaveAllValues();
    }
}


void GMainWindow::OnLoadAmiibo() {
    if (emu_thread == nullptr || !emu_thread->IsRunning()) {
        return;
    }
    if (is_amiibo_file_select_active) {
        return;
    }

    auto* virtual_amiibo = input_subsystem->GetVirtualAmiibo();

    // Remove amiibo if one is connected
    if (virtual_amiibo->GetCurrentState() == InputCommon::VirtualAmiibo::State::TagNearby) {
        virtual_amiibo->CloseAmiibo();
        QMessageBox::warning(this, tr("Amiibo"), tr("The current amiibo has been removed"));
        return;
    }

    if (virtual_amiibo->GetCurrentState() != InputCommon::VirtualAmiibo::State::WaitingForAmiibo) {
        QMessageBox::warning(this, tr("Error"), tr("The current game is not looking for amiibos"));
        return;
    }

    is_amiibo_file_select_active = true;
    const QString extensions{QStringLiteral("*.bin")};
    const QString file_filter = tr("Amiibo File (%1);; All Files (*.*)").arg(extensions);
    const QString filename = QFileDialog::getOpenFileName(this, tr("Load Amiibo"), {}, file_filter);
    is_amiibo_file_select_active = false;

    if (filename.isEmpty()) {
        return;
    }

    LoadAmiibo(filename);
}

bool GMainWindow::question(QWidget* parent, const QString& title, const QString& text,
                           QMessageBox::StandardButtons buttons,
                           QMessageBox::StandardButton defaultButton) {
    QMessageBox* box_dialog = new QMessageBox(parent);
    box_dialog->setWindowTitle(title);
    box_dialog->setText(text);
    box_dialog->setStandardButtons(buttons);
    box_dialog->setDefaultButton(defaultButton);

    ControllerNavigation* controller_navigation =
        new ControllerNavigation(system->HIDCore(), box_dialog);
    connect(controller_navigation, &ControllerNavigation::TriggerKeyboardEvent,
            [box_dialog](Qt::Key key) {
                QKeyEvent* event = new QKeyEvent(QEvent::KeyPress, key, Qt::NoModifier);
                QCoreApplication::postEvent(box_dialog, event);
            });
    int res = box_dialog->exec();

    controller_navigation->UnloadController();
    return res == QMessageBox::Yes;
}

void GMainWindow::LoadAmiibo(const QString& filename) {
    auto* virtual_amiibo = input_subsystem->GetVirtualAmiibo();
    const QString title = tr("Error loading Amiibo data");
    // Remove amiibo if one is connected
    if (virtual_amiibo->GetCurrentState() == InputCommon::VirtualAmiibo::State::TagNearby) {
        virtual_amiibo->CloseAmiibo();
        QMessageBox::warning(this, tr("Amiibo"), tr("The current amiibo has been removed"));
        return;
    }

    switch (virtual_amiibo->LoadAmiibo(filename.toStdString())) {
    case InputCommon::VirtualAmiibo::Info::NotAnAmiibo:
        QMessageBox::warning(this, title, tr("The selected file is not a valid amiibo"));
        break;
    case InputCommon::VirtualAmiibo::Info::UnableToLoad:
        QMessageBox::warning(this, title, tr("The selected file is already on use"));
        break;
    case InputCommon::VirtualAmiibo::Info::WrongDeviceState:
        QMessageBox::warning(this, title, tr("The current game is not looking for amiibos"));
        break;
    case InputCommon::VirtualAmiibo::Info::Unknown:
        QMessageBox::warning(this, title, tr("An unknown error occurred"));
        break;
    default:
        break;
    }
}

void GMainWindow::OnOpenSuyuFolder() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(
        QString::fromStdString(Common::FS::GetSuyuPathString(Common::FS::SuyuPath::SuyuDir))));
}

void GMainWindow::OnVerifyInstalledContents() {
    // Initialize a progress dialog.
    QProgressDialog progress(tr("Verifying integrity..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);

    // Declare progress callback.
    auto QtProgressCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    const std::vector<std::string> result =
        ContentManager::VerifyInstalledContents(*system, *provider, QtProgressCallback);
    progress.close();

    if (result.empty()) {
        QMessageBox::information(this, tr("Integrity verification succeeded!"),
                                 tr("The operation completed successfully."));
    } else {
        const auto failed_names =
            QString::fromStdString(fmt::format("{}", fmt::join(result, "\n")));
        QMessageBox::critical(
            this, tr("Integrity verification failed!"),
            tr("Verification failed for the following files:\n\n%1").arg(failed_names));
    }
}

void GMainWindow::OnInstallFirmware() {
    // Don't do this while emulation is running, that'd probably be a bad idea.
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    const QString firmware_source_location = QFileDialog::getExistingDirectory(
        this, tr("Select Dumped Firmware Source Location"), {}, QFileDialog::ShowDirsOnly);
    if (firmware_source_location.isEmpty()) {
        return;
    }

    QProgressDialog progress(tr("Installing Firmware..."), tr("Cancel"), 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(100);
    progress.setAutoClose(false);
    progress.setAutoReset(false);
    progress.show();

    // Declare progress callback.
    auto QtProgressCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(static_cast<int>((processed_size * 100) / total_size));
        return progress.wasCanceled();
    };

    LOG_INFO(Frontend, "Installing firmware from {}", firmware_source_location.toStdString());

    // Check for a reasonable number of .nca files (don't hardcode them, just see if there's some in
    // there.)
    std::filesystem::path firmware_source_path = firmware_source_location.toStdString();
    if (!Common::FS::IsDir(firmware_source_path)) {
        progress.close();
        return;
    }

    std::vector<std::filesystem::path> out;

    QtProgressCallback(100, 10);

    std::error_code walk_ec;
    for (const auto& entry :
         std::filesystem::recursive_directory_iterator(firmware_source_path, walk_ec)) {
        if (walk_ec) {
            break;
        }

        if (!entry.is_regular_file()) {
            continue;
        }

        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ext == ".nca") {
            out.emplace_back(entry.path());
        }
    }
    if (out.size() <= 0) {
        progress.close();
        QMessageBox::warning(this, tr("Firmware install failed"),
                             tr("Unable to locate potential firmware NCA files"));
        return;
    }

    // Locate and erase the content of nand/system/Content/registered/*.nca, if any.
    auto sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
    if (!sysnand_content_vdir) {
        LOG_ERROR(Frontend,
                  "System NAND content directory does not exist, attempting to create it.");
        // Try to trigger VFS factory creation so the directory becomes available.
        system->GetFileSystemController().CreateFactories(*vfs);
        sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
    }
    if (!sysnand_content_vdir) {
        progress.close();
        QMessageBox::critical(
            this, tr("Firmware install failed"),
            tr("Unable to access system NAND content directory. Ensure your NAND paths are set "
               "correctly in Settings > Filesystem."));
        return;
    }
    if (!sysnand_content_vdir->CleanSubdirectoryRecursive("registered")) {
        // If "registered" doesn't exist yet, that's okay — we'll create it below.
        LOG_WARNING(Frontend,
                    "CleanSubdirectoryRecursive(\"registered\") returned false — directory may "
                    "not exist yet.");
    }

    LOG_INFO(Frontend,
             "Cleaned nand/system/Content/registered folder in preparation for new firmware.");

    QtProgressCallback(100, 20);

    auto firmware_vdir = sysnand_content_vdir->GetDirectoryRelative("registered");
    if (!firmware_vdir) {
        firmware_vdir = sysnand_content_vdir->CreateDirectoryRelative("registered");
    }
    if (!firmware_vdir) {
        progress.close();
        QMessageBox::critical(this, tr("Firmware install failed"),
                              tr("Could not create the 'registered' directory in system NAND."));
        return;
    }

    bool success = true;
    int i = 0;
    try {
        for (const auto& firmware_src_path : out) {
            i++;
            auto firmware_src_vfile =
                vfs->OpenFile(firmware_src_path.generic_string(), FileSys::OpenMode::Read);
            auto firmware_dst_vfile =
                firmware_vdir->CreateFileRelative(firmware_src_path.filename().string());

            if (!VfsRawCopy(firmware_src_vfile, firmware_dst_vfile)) {
                LOG_ERROR(Frontend, "Failed to copy firmware file {} to {} in registered folder!",
                          firmware_src_path.generic_string(), firmware_src_path.filename().string());
                success = false;
            }

            if (QtProgressCallback(
                    100, 20 + static_cast<int>(((i) / static_cast<float>(out.size())) * 70.0))) {
                progress.close();
                QMessageBox::warning(
                    this, tr("Firmware install failed"),
                tr("Firmware installation cancelled, firmware may be in bad state, "
                   "restart suyu or re-install firmware."));
                return;
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception during firmware copy: {}", e.what());
        progress.close();
        QMessageBox::critical(this, tr("Firmware install failed"),
                              tr("An error occurred while copying firmware files:\n%1")
                                  .arg(QString::fromUtf8(e.what())));
        return;
    } catch (...) {
        LOG_ERROR(Frontend, "Unknown exception during firmware copy");
        progress.close();
        QMessageBox::critical(this, tr("Firmware install failed"),
                              tr("An unexpected error occurred while copying firmware files."));
        return;
    }

    if (!success) {
        progress.close();
        QMessageBox::critical(this, tr("Firmware install failed"),
                              tr("One or more firmware files failed to copy into NAND."));
        return;
    }

    // Re-scan VFS for the newly placed firmware files.
    try {
        system->GetFileSystemController().CreateFactories(*vfs);
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception rebuilding filesystem factories after firmware install: {}", e.what());
    } catch (...) {
        LOG_ERROR(Frontend, "Unknown exception rebuilding filesystem factories after firmware install");
    }

    auto VerifyFirmwareCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(90 + static_cast<int>((processed_size * 10) / total_size));
        return progress.wasCanceled();
    };

    if (ContentManager::AreKeysPresent()) {
        auto result =
            ContentManager::VerifyInstalledContents(*system, *provider, VerifyFirmwareCallback, true);

        if (result.size() > 0) {
            const auto failed_names =
                QString::fromStdString(fmt::format("{}", fmt::join(result, "\n")));
            progress.close();
            QMessageBox::critical(
                this, tr("Firmware integrity verification failed!"),
                tr("Verification failed for the following files:\n\n%1").arg(failed_names));
            return;
        }
    } else {
        LOG_WARNING(Frontend,
                    "Skipping firmware content verification because decryption keys are not available");
    }

    progress.close();
    OnCheckFirmwareDecryption();
}

void GMainWindow::OnInstallDecryptionKeys() {
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    const QString selected_path = QFileDialog::getOpenFileName(
        this, tr("Select prod.keys"),
        QString::fromStdString(Common::FS::GetSuyuPathString(Common::FS::SuyuPath::KeysDir)),
        tr("Keys (*.keys *.bin);;All files (*.*)"));

    if (selected_path.isEmpty()) {
        return;
    }

    QString error_message;
    int copied_count = 0;
    if (!InstallDecryptionKeysFromPath(selected_path, &error_message, &copied_count)) {
        QMessageBox::critical(this, tr("Key Installation Failed"), error_message);
        return;
    }

    QMessageBox::information(this, tr("Keys Installed"),
                             tr("Installed %1 key file(s).").arg(copied_count));
}

bool GMainWindow::InstallDecryptionKeysFromPath(const QString& key_source_location,
                                                QString* out_error, int* out_copied_count) {
    const std::filesystem::path source_path = key_source_location.toStdString();
    std::filesystem::path source_dir;
    if (Common::FS::IsDir(source_path)) {
        source_dir = source_path;
    } else {
        source_dir = source_path.parent_path();
    }

    if (!Common::FS::IsDir(source_dir)) {
        if (out_error != nullptr) {
            *out_error = tr("The selected key directory does not exist.");
        }
        return false;
    }

    const std::filesystem::path prod_key_path = source_dir / "prod.keys";
    if (!Common::FS::Exists(prod_key_path)) {
        if (out_error != nullptr) {
            *out_error = tr("prod.keys was not found in the selected directory.");
        }
        return false;
    }

    std::vector<std::filesystem::path> key_files{prod_key_path};
    const std::filesystem::path title_key_path = source_dir / "title.keys";
    if (Common::FS::Exists(title_key_path)) {
        key_files.emplace_back(title_key_path);
    }
    const std::filesystem::path retail_key_path = source_dir / "key_retail.bin";
    if (Common::FS::Exists(retail_key_path)) {
        key_files.emplace_back(retail_key_path);
    }

    const auto destination_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::KeysDir);
    std::filesystem::create_directories(destination_dir);
    int copied_count = 0;
    try {
        for (const auto& key_file : key_files) {
            const std::filesystem::path destination_file = destination_dir / key_file.filename();
            if (Common::FS::Exists(destination_file)) {
                std::error_code equivalent_ec;
                if (std::filesystem::equivalent(key_file, destination_file, equivalent_ec) &&
                    !equivalent_ec) {
                    ++copied_count;
                    continue;
                }
            }

            std::error_code copy_ec;
            if (!std::filesystem::copy_file(key_file, destination_file,
                                            std::filesystem::copy_options::overwrite_existing,
                                            copy_ec) ||
                copy_ec) {
                if (out_error != nullptr) {
                    *out_error = tr("Failed to copy one or more key files:\n%1")
                                     .arg(QString::fromStdString(copy_ec.message()));
                }
                return false;
            }
            ++copied_count;
        }

        game_list->CancelPopulate();
        Core::Crypto::KeyManager::Instance().ReloadKeys();
        system->GetFileSystemController().CreateFactories(*vfs);
        game_list->PopulateAsync(UISettings::values.game_dirs);
        OnCheckFirmwareDecryption();

        if (out_copied_count != nullptr) {
            *out_copied_count = copied_count;
        }
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(Frontend, "Exception installing decryption keys: {}", e.what());
        if (out_error != nullptr) {
            *out_error = tr("An error occurred while installing keys:\n%1")
                             .arg(QString::fromUtf8(e.what()));
        }
        return false;
    } catch (...) {
        LOG_ERROR(Frontend, "Unknown exception installing decryption keys");
        if (out_error != nullptr) {
            *out_error = tr("An unexpected error occurred while installing keys.");
        }
        return false;
    }
}

void GMainWindow::OnConfigureExternalDecryption() {
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    if (!external_decryption_tool_) {
        try {
            external_decryption_tool_ = new ExternalDecryptionTool(this);
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "Exception creating ExternalDecryptionTool: {}", e.what());
            QMessageBox::critical(this, tr("Decryption Tool Error"),
                                  tr("Failed to initialize the external decryption tool:\n%1")
                                      .arg(QString::fromUtf8(e.what())));
            return;
        } catch (...) {
            LOG_ERROR(Frontend, "Unknown exception creating ExternalDecryptionTool");
            QMessageBox::critical(this, tr("Decryption Tool Error"),
                                  tr("An unexpected error occurred initializing the external decryption tool."));
            return;
        }
    }

    ExternalDecryptionToolDialog dialog(external_decryption_tool_, this);
    if (dialog.exec() == QDialog::Accepted) {
        // Re-check keys and refresh state after tool configuration
        try {
            game_list->CancelPopulate();
            Core::Crypto::KeyManager::Instance().ReloadKeys();
            system->GetFileSystemController().CreateFactories(*vfs);
            game_list->PopulateAsync(UISettings::values.game_dirs);
            OnCheckFirmwareDecryption();
        } catch (const std::exception& e) {
            LOG_ERROR(Frontend, "Exception reloading keys after external decryption tool: {}", e.what());
            QMessageBox::critical(this, tr("Decryption Tool Error"),
                                  tr("Failed to reload keys after tool configuration:\n%1")
                                      .arg(QString::fromUtf8(e.what())));
        } catch (...) {
            LOG_ERROR(Frontend, "Unknown exception reloading keys after external decryption tool");
        }
    }
}

void GMainWindow::ApplyAppMode(AppMode mode) {
    current_mode_ = mode;
    // --- Gamer Environment (new card-grid UI) ---
    if (mode == AppMode::Gamer) {
        if (!gamer_env_) {
            gamer_env_ = new GamerEnvironment(game_list, this);
            ui->emulationLayout->addWidget(gamer_env_);
            connect(gamer_env_, &GamerEnvironment::GameLaunchRequested,
                    this, [this](const QString& path) {
                        if (path.isEmpty() || path.startsWith(QStringLiteral("owned://")) ||
                            !QFileInfo::exists(path)) {
                            QMessageBox::information(
                                this, tr("Launch Game"),
                                tr("This library entry is not launchable yet. Add a local ROM or "
                                   "decrypted game folder to your library and try again."));
                            return;
                        }
                        BootGame(path, ApplicationAppletParameters());
                    });
            connect(gamer_env_, &GamerEnvironment::AddDirectoryRequested,
                    this, &GMainWindow::OnGameListAddDirectory);
            connect(gamer_env_, &GamerEnvironment::LoadFileRequested,
                    this, &GMainWindow::OnMenuLoadFile);
            connect(gamer_env_, &GamerEnvironment::OpenSettingsRequested,
                    this, &GMainWindow::OnConfigure);
            connect(gamer_env_, &GamerEnvironment::OpenMultiplayerRequested,
                    this, [this]() {
                        if (multiplayer_state) multiplayer_state->OnViewLobby();
                    });
                connect(gamer_env_, &GamerEnvironment::OpenUserManualRequested,
                        this, &GMainWindow::OnOpenUserManual);
        }
        game_list->hide();
        game_list_placeholder->hide();
        if (programmer_env_) programmer_env_->hide();
        if (!emulation_running) {
            gamer_env_->show();
            gamer_env_->RefreshGameGrid();
        } else {
            gamer_env_->hide();
        }
    } else {
        if (gamer_env_) gamer_env_->hide();
    }

    // --- Programmer Environment ---
    if (mode == AppMode::Programmer) {
        if (!programmer_env_) {
            programmer_env_ = new ProgrammerEnvironment(this);
            ui->emulationLayout->addWidget(programmer_env_);
        }
        game_list->hide();
        game_list_placeholder->hide();
        if (!emulation_running) {
            programmer_env_->show();
        } else {
            programmer_env_->hide();
        }
    } else {
        if (programmer_env_ && mode != AppMode::Gamer)
            programmer_env_->hide();
    }

    // --- Legacy game list (Hacker mode only) ---
    if (mode == AppMode::Hacker) {
        if (!emulation_running) {
            game_list->show();
        }
    }

    // --- Social Sidebar dock (suppressed - social is inside GamerEnvironment now) ---
    if (social_sidebar_dock_) social_sidebar_dock_->setVisible(false);

    // --- MCP Server (available in all modes) ---
    const bool allow_runtime_mcp = !emulation_running;
    if (allow_runtime_mcp && !mcp_server_) {
        mcp_server_ = new McpServer(this);
        mcp_server_->SetStateProvider([this]() -> QJsonObject {
            QJsonObject state;
            if (gamer_env_ && gamer_env_->isVisible()) {
                const QJsonObject gamer_state = gamer_env_->GetMcpState();
                state[QStringLiteral("mode")] = QStringLiteral("gamer");
                state[QStringLiteral("game_count")] = gamer_state.value(QStringLiteral("visible_game_count")).toInt();
                state[QStringLiteral("search_filter")] = gamer_state.value(QStringLiteral("search_filter")).toString();
                state[QStringLiteral("current_view")] = gamer_state.value(QStringLiteral("current_view")).toString();
                state[QStringLiteral("social_feed_status")] =
                    gamer_state.value(QStringLiteral("social_feed_status")).toString();
                state[QStringLiteral("social_feed_error")] =
                    gamer_state.value(QStringLiteral("social_feed_error")).toString();
                state[QStringLiteral("social_post_count")] =
                    gamer_state.value(QStringLiteral("social_post_count")).toInt();
                state[QStringLiteral("social_last_updated")] =
                    gamer_state.value(QStringLiteral("social_last_updated")).toString();
            } else if (hacker_env_ && hacker_env_dock_ && hacker_env_dock_->isVisible()) {
                state[QStringLiteral("mode")] = QStringLiteral("hacker");
            } else if (programmer_env_ && programmer_env_->isVisible()) {
                state[QStringLiteral("mode")] = QStringLiteral("programmer");
            } else {
                state[QStringLiteral("mode")] = QStringLiteral("unknown");
            }
            state[QStringLiteral("game_running")] =
                emulation_running ||
                (core_manager_ ? core_manager_->IsGameRunning() : false);
            state[QStringLiteral("emu_running")] = emulation_running;
            state[QStringLiteral("emulation_thread_running")] =
                emu_thread ? emu_thread->IsRunning() : false;
            state[QStringLiteral("first_frame_displayed")] =
                render_window ? render_window->IsLoadingComplete() : false;
            state[QStringLiteral("qt_ssl_available")] = qt_ssl_available_;
            state[QStringLiteral("qt_ssl_build_version")] = qt_ssl_build_version_;
            state[QStringLiteral("qt_ssl_runtime_version")] = qt_ssl_runtime_version_;
            return state;
        });
    }
    if (allow_runtime_mcp && mcp_server_ && !mcp_server_->IsRunning()) {
        if (!mcp_server_->Start(9742)) {
            LOG_ERROR(Frontend, "MCP Server failed to start on port 9742: {}",
                      mcp_server_->GetLastErrorString().toStdString());
        } else {
            LOG_INFO(Frontend, "MCP Server started on port 9742");
            const auto install_firmware_from_directory = [this](const QString& firmware_source_location) -> QJsonObject {
                if (emu_thread != nullptr && emu_thread->IsRunning()) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("Cannot install firmware while emulation is running")}};
                }

                const std::filesystem::path firmware_source_path = firmware_source_location.toStdString();
                if (firmware_source_location.isEmpty() || !Common::FS::IsDir(firmware_source_path)) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("Firmware source directory does not exist")},
                                       {QStringLiteral("path"), firmware_source_location}};
                }

                std::vector<std::filesystem::path> nca_files;
                std::error_code walk_ec;
                for (const auto& entry :
                     std::filesystem::recursive_directory_iterator(firmware_source_path, walk_ec)) {
                    if (walk_ec) {
                        break;
                    }
                    if (!entry.is_regular_file()) {
                        continue;
                    }

                    auto ext = entry.path().extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(),
                                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                    if (ext == ".nca") {
                        nca_files.emplace_back(entry.path());
                    }
                }

                if (nca_files.empty()) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("No firmware NCA files were found")},
                                       {QStringLiteral("path"), firmware_source_location}};
                }

                auto sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
                if (!sysnand_content_vdir) {
                    system->GetFileSystemController().CreateFactories(*vfs);
                    sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
                }
                if (!sysnand_content_vdir) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("System NAND content directory not available")}};
                }
                if (!sysnand_content_vdir->CleanSubdirectoryRecursive("registered")) {
                    LOG_WARNING(Frontend,
                                "CleanSubdirectoryRecursive(\"registered\") returned false — "
                                "directory may not exist yet.");
                }

                auto firmware_vdir = sysnand_content_vdir->GetDirectoryRelative("registered");
                if (!firmware_vdir) {
                    firmware_vdir = sysnand_content_vdir->CreateDirectoryRelative("registered");
                }
                if (!firmware_vdir) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("Could not create 'registered' directory")}};
                }
                int copied_count = 0;
                QJsonArray copy_failures;
                for (const auto& firmware_src_path : nca_files) {
                    auto firmware_src_vfile =
                        vfs->OpenFile(firmware_src_path.generic_string(), FileSys::OpenMode::Read);
                    auto firmware_dst_vfile =
                        firmware_vdir->CreateFileRelative(firmware_src_path.filename().string());
                    if (!VfsRawCopy(firmware_src_vfile, firmware_dst_vfile)) {
                        copy_failures.append(QString::fromStdString(firmware_src_path.filename().string()));
                        continue;
                    }
                    ++copied_count;
                }

                if (!copy_failures.isEmpty()) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("One or more firmware files failed to copy")},
                                       {QStringLiteral("copied_count"), copied_count},
                                       {QStringLiteral("copy_failures"), copy_failures}};
                }

                system->GetFileSystemController().CreateFactories(*vfs);
                if (ContentManager::AreKeysPresent()) {
                    const auto verify_callback = [](size_t /*total*/, size_t /*processed*/) {
                        return false;
                    };
                    const auto result =
                        ContentManager::VerifyInstalledContents(*system, *provider, verify_callback, true);

                    if (!result.empty()) {
                        QJsonArray verification_failures;
                        for (const auto& failed : result) {
                            verification_failures.append(QString::fromStdString(failed));
                        }
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Firmware verification failed")},
                                           {QStringLiteral("copied_count"), copied_count},
                                           {QStringLiteral("verification_failures"), verification_failures}};
                    }
                }

                OnCheckFirmwareDecryption();
                return QJsonObject{{QStringLiteral("success"), true},
                                   {QStringLiteral("path"), firmware_source_location},
                                   {QStringLiteral("detected_nca_count"), static_cast<int>(nca_files.size())},
                                   {QStringLiteral("copied_count"), copied_count}};
            };

            // Register runtime tools that need access to UISettings / game_list
            mcp_server_->RegisterTool(
                QStringLiteral("capture_ui_screenshot"),
                QStringLiteral("Capture a PNG screenshot of the current main window or active modal dialog."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("path"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Optional output PNG path")}}},
                                         {QStringLiteral("target"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("main_window, active_modal, active_window, or central_widget")}}}}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    const QString target = params[QStringLiteral("target")].toString(QStringLiteral("main_window"));
                    QString output_path = params[QStringLiteral("path")].toString();
                    QWidget* target_widget = this;

                    if (target == QStringLiteral("active_modal")) {
                        target_widget = QApplication::activeModalWidget();
                    } else if (target == QStringLiteral("active_window")) {
                        target_widget = QApplication::activeWindow();
                    } else if (target == QStringLiteral("central_widget")) {
                        target_widget = centralWidget();
                    }

                    if (!target_widget) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Requested screenshot target is not available")},
                                           {QStringLiteral("target"), target}};
                    }

                    if (output_path.isEmpty()) {
                        output_path = QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                          .filePath(QStringLiteral("suyu_mcp_%1.png")
                                                        .arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_hhmmss_zzz"))));
                    }

                    QFileInfo file_info(output_path);
                    QDir().mkpath(file_info.absolutePath());

                    const QPixmap screenshot = target_widget->grab();
                    if (screenshot.isNull() || !screenshot.save(output_path, "PNG")) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Failed to save screenshot")},
                                           {QStringLiteral("path"), output_path}};
                    }

                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("path"), output_path},
                                       {QStringLiteral("target"), target},
                                       {QStringLiteral("width"), screenshot.width()},
                                       {QStringLiteral("height"), screenshot.height()}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("set_app_mode"),
                QStringLiteral("Switch the active suyu interface mode to gamer, programmer, or hacker."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("mode"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Mode name: gamer, programmer, or hacker")}}}}},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("mode")}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    const QString mode = params[QStringLiteral("mode")].toString().trimmed().toLower();
                    if (mode == QStringLiteral("gamer")) {
                        ApplyAppMode(AppMode::Gamer);
                    } else if (mode == QStringLiteral("programmer")) {
                        ApplyAppMode(AppMode::Programmer);
                    } else if (mode == QStringLiteral("hacker")) {
                        ApplyAppMode(AppMode::Hacker);
                    } else {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"), QStringLiteral("Unknown mode: %1").arg(mode)}};
                    }

                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("mode"), mode}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("navigate_gamer_view"),
                QStringLiteral("Navigate the gamer interface to the library or social view, or open related dialogs."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("view"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("library, social, settings, multiplayer, manual, website, or more_options")}}}}},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("view")}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    const QString view = params[QStringLiteral("view")].toString().trimmed().toLower();
                    if (current_mode_ != AppMode::Gamer) {
                        ApplyAppMode(AppMode::Gamer);
                    }
                    if (!gamer_env_) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"), QStringLiteral("Gamer environment is not available")}};
                    }

                    bool invoked = false;
                    if (view == QStringLiteral("library")) {
                        invoked = QMetaObject::invokeMethod(gamer_env_, "OnNavLibraryClicked", Qt::DirectConnection);
                    } else if (view == QStringLiteral("social")) {
                        invoked = QMetaObject::invokeMethod(gamer_env_, "OnNavSocialClicked", Qt::DirectConnection);
                    } else if (view == QStringLiteral("settings")) {
                        OnConfigure();
                        invoked = true;
                    } else if (view == QStringLiteral("multiplayer")) {
                        if (multiplayer_state) {
                            multiplayer_state->OnViewLobby();
                            invoked = true;
                        }
                    } else if (view == QStringLiteral("manual")) {
                        OnOpenUserManual();
                        invoked = true;
                    } else if (view == QStringLiteral("website")) {
                        QMetaObject::invokeMethod(gamer_env_, "OnNavWebsiteClicked", Qt::DirectConnection);
                        invoked = true;
                    } else if (view == QStringLiteral("more_options")) {
                        invoked = QMetaObject::invokeMethod(gamer_env_, "OnNavMoreOptionsClicked", Qt::DirectConnection);
                    }

                    if (!invoked) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"), QStringLiteral("Unsupported gamer view: %1").arg(view)}};
                    }

                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("view"), view}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("refresh_social_feed"),
                QStringLiteral("Refresh the Reddit-based social feed in gamer mode."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{}}},
                [this](const QJsonObject& /*params*/) -> QJsonObject {
                    if (current_mode_ != AppMode::Gamer) {
                        ApplyAppMode(AppMode::Gamer);
                    }
                    if (!gamer_env_) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"), QStringLiteral("Gamer environment is not available")}};
                    }
                    gamer_env_->RefreshSocialFeed();
                    return QJsonObject{{QStringLiteral("success"), true}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("get_lobby_row_counts"),
                QStringLiteral("Fetch multiplayer lobby row counts after filters for smoke tests."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{}}},
                [this](const QJsonObject& /*params*/) -> QJsonObject {
                    if (!multiplayer_state) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Multiplayer state is not available")}};
                    }

                    multiplayer_state->OnViewLobby();

                    // Wait for the async room list fetch to complete (up to 15s)
                    QWidget* lobby_window = nullptr;
                    for (int attempt = 0; attempt < 30; ++attempt) {
                        QThread::msleep(500);
                        QCoreApplication::processEvents();

                        const auto top_level_widgets = QApplication::topLevelWidgets();
                        for (QWidget* widget : top_level_widgets) {
                            if (!widget) continue;
                            if (widget->objectName() == QStringLiteral("Lobby") ||
                                widget->windowTitle().contains(QStringLiteral("Public Room Browser"),
                                                               Qt::CaseInsensitive)) {
                                lobby_window = widget;
                                break;
                            }
                        }
                        if (!lobby_window) continue;

                        auto* room_list =
                            lobby_window->findChild<QTreeView*>(QStringLiteral("room_list"));
                        if (room_list && room_list->model()) {
                            int unfiltered = 0;
                            if (const auto* proxy =
                                    qobject_cast<const QSortFilterProxyModel*>(room_list->model())) {
                                if (proxy->sourceModel())
                                    unfiltered = proxy->sourceModel()->rowCount();
                            } else {
                                unfiltered = room_list->model()->rowCount();
                            }
                            if (unfiltered > 0) break;
                        }
                    }

                    if (!lobby_window) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Lobby window is not available")}};
                    }

                    auto* room_list = lobby_window->findChild<QTreeView*>(QStringLiteral("room_list"));
                    if (!room_list || !room_list->model()) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Lobby room list model is not available")}};
                    }

                    const int visible_rows = room_list->model()->rowCount();
                    int unfiltered_rows = visible_rows;
                    if (const auto* proxy_model =
                            qobject_cast<const QSortFilterProxyModel*>(room_list->model())) {
                        if (proxy_model->sourceModel()) {
                            unfiltered_rows = proxy_model->sourceModel()->rowCount();
                        }
                    }

                    const auto* search =
                        lobby_window->findChild<QLineEdit*>(QStringLiteral("search"));
                    const auto* games_owned =
                        lobby_window->findChild<QCheckBox*>(QStringLiteral("games_owned"));
                    const auto* hide_empty =
                        lobby_window->findChild<QCheckBox*>(QStringLiteral("hide_empty"));
                    const auto* hide_full =
                        lobby_window->findChild<QCheckBox*>(QStringLiteral("hide_full"));

                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("visible_rows"), visible_rows},
                                       {QStringLiteral("unfiltered_rows"), unfiltered_rows},
                                       {QStringLiteral("filters"),
                                        QJsonObject{{QStringLiteral("search"),
                                                     search ? search->text() : QString()},
                                                    {QStringLiteral("games_owned"),
                                                     games_owned ? games_owned->isChecked() : false},
                                                    {QStringLiteral("hide_empty"),
                                                     hide_empty ? hide_empty->isChecked() : false},
                                                    {QStringLiteral("hide_full"),
                                                     hide_full ? hide_full->isChecked() : false}}}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("set_theme_mode"),
                QStringLiteral("Set the frontend theme mode to light, dark, or auto and apply it immediately."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("mode"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Theme mode: light, dark, or auto")}}}}},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("mode")}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    const QString mode = params[QStringLiteral("mode")].toString().trimmed().toLower();
                    if (mode == QStringLiteral("dark")) {
                        UISettings::values.dark_mode_state = DarkModeState::On;
                    } else if (mode == QStringLiteral("light")) {
                        UISettings::values.dark_mode_state = DarkModeState::Off;
                    } else if (mode == QStringLiteral("auto")) {
                        UISettings::values.dark_mode_state = DarkModeState::Auto;
                    } else {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"), QStringLiteral("Unknown theme mode: %1").arg(mode)}};
                    }
                    UpdateUITheme();
                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("mode"), mode},
                                       {QStringLiteral("is_dark_mode"), CheckDarkMode()}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("trigger_ui_action"),
                QStringLiteral("Trigger a named frontend action without using menu automation."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("action"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Action name such as export_game, open_user_manual, nintendo_account, steam_integration, configure, toggle_fullscreen, or install_firmware_dialog")}}}}},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("action")}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    const QString action = params[QStringLiteral("action")].toString().trimmed().toLower();
                    if (action == QStringLiteral("export_game")) {
                        OnExportGame();
                    } else if (action == QStringLiteral("open_user_manual")) {
                        OnOpenUserManual();
                    } else if (action == QStringLiteral("nintendo_account")) {
                        OnNintendoAccount();
                    } else if (action == QStringLiteral("steam_integration")) {
                        OnSteamIntegration();
                    } else if (action == QStringLiteral("configure")) {
                        OnConfigure();
                    } else if (action == QStringLiteral("toggle_fullscreen")) {
                        ToggleFullscreen();
                    } else if (action == QStringLiteral("install_firmware_dialog")) {
                        OnInstallFirmware();
                    } else if (action == QStringLiteral("install_keys_dialog")) {
                        OnInstallDecryptionKeys();
                    } else {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"), QStringLiteral("Unknown action: %1").arg(action)}};
                    }
                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("action"), action}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("launch_game_path"),
                QStringLiteral("Launch a local ROM or deconstructed game directory by absolute path."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("path"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Absolute path to a ROM file or game directory")}}}}},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    if (emulation_running) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Emulation is already running")}};
                    }

                    const QString path = params[QStringLiteral("path")].toString().trimmed();
                    const QFileInfo info(path);
                    if (path.isEmpty() || !info.exists()) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Game path does not exist")},
                                           {QStringLiteral("path"), path}};
                    }

                    BootGame(path, ApplicationAppletParameters());
                    return QJsonObject{{QStringLiteral("success"), emulation_running},
                                       {QStringLiteral("path"), path},
                                       {QStringLiteral("emu_running"), emulation_running}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("stop_emulation"),
                QStringLiteral("Stop the currently running emulation session."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{}}},
                [this](const QJsonObject& /*params*/) -> QJsonObject {
                    const bool was_running = emulation_running;
                    if (emulation_running) {
                        // Programmatic stop: skip the interactive confirm dialog,
                        // which would block the GUI thread waiting for a click.
                        play_time_manager->Stop();
                        game_list->PopulateAsync(UISettings::values.game_dirs);
                        if (OnShutdownBegin()) {
                            OnShutdownBeginDialog();
                        } else {
                            OnEmulationStopped();
                        }
                    }
                    return QJsonObject{{QStringLiteral("success"), true},
                                       {QStringLiteral("was_running"), was_running},
                                       {QStringLiteral("emu_running"), emulation_running}};
                });

            mcp_server_->RegisterTool(
                QStringLiteral("get_thread_diagnostics"),
                QStringLiteral("Return guest thread states, registers, and short backtraces for launch debugging."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("include_backtrace"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("boolean")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Include a short guest callstack for each thread")}}},
                                         {QStringLiteral("max_backtrace"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("integer")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Maximum backtrace frames per thread")}}}}}},
                [this](const QJsonObject& params) -> QJsonObject {
                    QJsonObject result;
                    result[QStringLiteral("emu_running")] = emulation_running;
                    result[QStringLiteral("emulation_thread_running")] =
                        emu_thread ? emu_thread->IsRunning() : false;
                    result[QStringLiteral("first_frame_displayed")] =
                        render_window ? render_window->IsLoadingComplete() : false;

                    auto* process = system->ApplicationProcess();
                    if (!process) {
                        result[QStringLiteral("error")] =
                            QStringLiteral("No application process is loaded");
                        return result;
                    }

                    const bool include_backtrace =
                        params[QStringLiteral("include_backtrace")].toBool(true);
                    const int max_backtrace =
                        qBound(0, params[QStringLiteral("max_backtrace")].toInt(8), 32);

                    const auto hex64 = [](u64 value) {
                        return QStringLiteral("0x%1").arg(static_cast<qulonglong>(value), 0, 16);
                    };

                    QJsonArray threads;
                    for (const auto* thread : system->GlobalSchedulerContext().GetThreadList()) {
                        if (!thread || thread->GetThreadType() != Kernel::ThreadType::User ||
                            thread->GetOwnerProcess() != process) {
                            continue;
                        }

                        const auto& ctx = thread->GetContext();
                        QJsonObject thread_json;
                        thread_json[QStringLiteral("thread_id")] = hex64(thread->GetThreadId());
                        thread_json[QStringLiteral("name")] = QString::fromStdString(
                            Core::GetThreadName(thread).value_or(std::string{}));
                        thread_json[QStringLiteral("state")] =
                            QString::fromStdString(Core::GetThreadState(thread));
                        thread_json[QStringLiteral("wait_reason")] =
                            QString::fromUtf8(Core::GetThreadWaitReason(thread).data(),
                                              static_cast<int>(
                                                  Core::GetThreadWaitReason(thread).size()));
                        thread_json[QStringLiteral("active_core")] = thread->GetActiveCore();
                        thread_json[QStringLiteral("priority")] = thread->GetPriority();
                        thread_json[QStringLiteral("base_priority")] = thread->GetBasePriority();
                        thread_json[QStringLiteral("last_scheduled_tick")] =
                            QString::number(thread->GetLastScheduledTick());
                        thread_json[QStringLiteral("pc")] = hex64(ctx.pc);
                        thread_json[QStringLiteral("lr")] = hex64(ctx.lr);
                        thread_json[QStringLiteral("sp")] = hex64(ctx.sp);
                        thread_json[QStringLiteral("fp")] = hex64(ctx.fp);

                        if (include_backtrace && max_backtrace > 0) {
                            QJsonArray frames;
                            const auto backtrace = Core::GetBacktrace(thread);
                            const int frame_count =
                                std::min<int>(static_cast<int>(backtrace.size()), max_backtrace);
                            for (int i = 0; i < frame_count; ++i) {
                                const auto& frame = backtrace[static_cast<size_t>(i)];
                                QJsonObject frame_json;
                                frame_json[QStringLiteral("address")] =
                                    hex64(frame.original_address);
                                frame_json[QStringLiteral("module")] =
                                    QString::fromStdString(frame.module);
                                frame_json[QStringLiteral("symbol")] =
                                    QString::fromStdString(frame.name);
                                frame_json[QStringLiteral("offset")] = hex64(frame.offset);
                                frames.append(frame_json);
                            }
                            thread_json[QStringLiteral("backtrace")] = frames;
                        }

                        threads.append(thread_json);
                    }

                    result[QStringLiteral("thread_count")] = threads.size();
                    result[QStringLiteral("threads")] = threads;
                    return result;
                });

            mcp_server_->RegisterTool(
                QStringLiteral("install_firmware_from_path"),
                QStringLiteral("Install dumped firmware directly from a directory path without opening the folder picker dialog."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"),
                             QJsonObject{{QStringLiteral("path"),
                                          QJsonObject{{QStringLiteral("type"), QStringLiteral("string")},
                                                      {QStringLiteral("description"),
                                                       QStringLiteral("Absolute path to the dumped firmware directory")}}}}},
                            {QStringLiteral("required"), QJsonArray{QStringLiteral("path")}}},
                [install_firmware_from_directory](const QJsonObject& params) -> QJsonObject {
                    return install_firmware_from_directory(params[QStringLiteral("path")].toString());
                });

            mcp_server_->RegisterTool(
                QStringLiteral("add_game_directory"),
                QStringLiteral("Add a directory to the game library scan list."),
                []() -> QJsonObject {
                    QJsonObject props;
                    props[QStringLiteral("path")] = QJsonObject{
                        {QStringLiteral("type"), QStringLiteral("string")},
                        {QStringLiteral("description"),
                         QStringLiteral("Absolute path to the directory containing games")}};
                    QJsonObject schema;
                    schema[QStringLiteral("type")] = QStringLiteral("object");
                    schema[QStringLiteral("properties")] = props;
                    schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("path")};
                    return schema;
                }(),
                [this](const QJsonObject& params) -> QJsonObject {
                    const QString dir_path = params[QStringLiteral("path")].toString();
                    if (dir_path.isEmpty()) {
                        return QJsonObject{{QStringLiteral("error"),
                                           QStringLiteral("path parameter is required")}};
                    }
                    if (!QDir(dir_path).exists()) {
                        return QJsonObject{
                            {QStringLiteral("error"),
                             QStringLiteral("Directory does not exist: %1").arg(dir_path)}};
                    }
                    UISettings::GameDir game_dir{dir_path.toStdString(), true, true};
                    const bool already_present =
                        UISettings::values.game_dirs.contains(game_dir);
                    if (!already_present) {
                        UISettings::values.game_dirs.append(game_dir);
                        if (game_list) {
                            game_list->PopulateAsync(UISettings::values.game_dirs);
                        }
                    }
                    return QJsonObject{
                        {QStringLiteral("success"), true},
                        {QStringLiteral("path"), dir_path},
                        {QStringLiteral("already_present"), already_present},
                        {QStringLiteral("total_directories"),
                         UISettings::values.game_dirs.size()},
                    };
                });

            mcp_server_->RegisterTool(
                QStringLiteral("list_configured_game_dirs"),
                QStringLiteral(
                    "List all directories currently configured for game library scanning."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{}}},
                [](const QJsonObject& /*params*/) -> QJsonObject {
                    QJsonArray dirs;
                    for (const auto& gd : UISettings::values.game_dirs) {
                        QJsonObject entry;
                        entry[QStringLiteral("path")] =
                            QString::fromStdString(gd.path);
                        entry[QStringLiteral("deep_scan")] = gd.deep_scan;
                        entry[QStringLiteral("expanded")] = gd.expanded;
                        dirs.append(entry);
                    }
                    return QJsonObject{
                        {QStringLiteral("count"), dirs.size()},
                        {QStringLiteral("directories"), dirs},
                    };
                });

            mcp_server_->RegisterTool(
                QStringLiteral("refresh_game_library"),
                QStringLiteral("Refresh the game library grid in gamer mode."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{}}},
                [this](const QJsonObject& /*params*/) -> QJsonObject {
                    if (current_mode_ != AppMode::Gamer) {
                        ApplyAppMode(AppMode::Gamer);
                    }
                    if (!gamer_env_) {
                        return QJsonObject{{QStringLiteral("success"), false},
                                           {QStringLiteral("error"),
                                            QStringLiteral("Gamer environment is not available")}};
                    }
                    gamer_env_->RefreshGameGrid();
                    return QJsonObject{{QStringLiteral("success"), true}};
                });

            {
                QJsonObject filter_schema;
                filter_schema[QStringLiteral("type")] = QStringLiteral("string");
                filter_schema[QStringLiteral("description")] =
                    QStringLiteral("Text to filter visible games by title");

                QJsonObject properties;
                properties[QStringLiteral("filter")] = filter_schema;

                QJsonObject search_filter_schema;
                search_filter_schema[QStringLiteral("type")] = QStringLiteral("object");
                search_filter_schema[QStringLiteral("properties")] = properties;
                search_filter_schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("filter")};

                mcp_server_->RegisterTool(
                    QStringLiteral("set_gamer_search_filter"),
                    QStringLiteral("Set the gamer library search filter and refresh the visible library."),
                    search_filter_schema,
                    [this](const QJsonObject& params) -> QJsonObject {
                        const QString filter = params[QStringLiteral("filter")].toString();
                        if (current_mode_ != AppMode::Gamer) {
                            ApplyAppMode(AppMode::Gamer);
                        }
                        if (!gamer_env_) {
                            return QJsonObject{{QStringLiteral("success"), false},
                                               {QStringLiteral("error"),
                                                QStringLiteral("Gamer environment is not available")}};
                        }
                        const bool invoked = QMetaObject::invokeMethod(
                            gamer_env_, "OnSearchChanged", Qt::DirectConnection,
                            Q_ARG(QString, filter));
                        if (!invoked) {
                            return QJsonObject{{QStringLiteral("success"), false},
                                               {QStringLiteral("error"),
                                                QStringLiteral("Failed to set the search filter")}};
                        }
                        return QJsonObject{{QStringLiteral("success"), true},
                                           {QStringLiteral("filter"), filter}};
                    });
            }

            const auto install_keys_from_directory = [this](const QString& key_source_location) -> QJsonObject {
                if (emu_thread != nullptr && emu_thread->IsRunning()) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"),
                                        QStringLiteral("Cannot install keys while emulation is running")}};
                }

                QString error_message;
                int copied_count = 0;
                if (!InstallDecryptionKeysFromPath(key_source_location, &error_message,
                                                   &copied_count)) {
                    return QJsonObject{{QStringLiteral("success"), false},
                                       {QStringLiteral("error"), error_message}};
                }

                return QJsonObject{{QStringLiteral("success"), true},
                                   {QStringLiteral("copied_count"), copied_count}};
            };

            {
                QJsonObject path_schema;
                path_schema[QStringLiteral("type")] = QStringLiteral("string");
                path_schema[QStringLiteral("description")] =
                    QStringLiteral("Absolute path to prod.keys or a directory containing key files");

                QJsonObject properties;
                properties[QStringLiteral("path")] = path_schema;

                QJsonObject install_keys_schema;
                install_keys_schema[QStringLiteral("type")] = QStringLiteral("object");
                install_keys_schema[QStringLiteral("properties")] = properties;
                install_keys_schema[QStringLiteral("required")] = QJsonArray{QStringLiteral("path")};

                mcp_server_->RegisterTool(
                    QStringLiteral("install_keys_from_path"),
                    QStringLiteral("Install prod.keys/title.keys from a file path or directory."),
                    install_keys_schema,
                    [install_keys_from_directory](const QJsonObject& params) -> QJsonObject {
                        return install_keys_from_directory(params[QStringLiteral("path")].toString());
                    });
            }

            mcp_server_->RegisterTool(
                QStringLiteral("get_firmware_status"),
                QStringLiteral("Get the current firmware and keys installation status."),
                QJsonObject{{QStringLiteral("type"), QStringLiteral("object")},
                            {QStringLiteral("properties"), QJsonObject{}}},
                [](const QJsonObject& /*params*/) -> QJsonObject {
                    const auto nand_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::NANDDir);
                    const auto keys_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::KeysDir);
                    QJsonObject result;
                    result[QStringLiteral("nand_path")] =
                        QString::fromStdString(nand_dir.string());
                    result[QStringLiteral("nand_exists")] =
                        Common::FS::IsDir(nand_dir);
                    result[QStringLiteral("keys_path")] =
                        QString::fromStdString(keys_dir.string());
                    result[QStringLiteral("keys_dir_exists")] =
                        Common::FS::IsDir(keys_dir);
                    result[QStringLiteral("prod_keys_present")] =
                        Common::FS::Exists(keys_dir / "prod.keys") ? true : false;
                    result[QStringLiteral("title_keys_present")] =
                        Common::FS::Exists(keys_dir / "title.keys") ? true : false;
                    return result;
                });
        }
    }

    // --- Hacker Environment dock ---
    if (mode == AppMode::Hacker) {
        if (!hacker_env_) {
            hacker_env_ = new HackerEnvironment(this);
            hacker_env_dock_ = new QDockWidget(tr("Hacker Tools"), this);
            hacker_env_dock_->setObjectName(QStringLiteral("HackerDock"));
            hacker_env_dock_->setWidget(hacker_env_);
            addDockWidget(Qt::BottomDockWidgetArea, hacker_env_dock_);
        }
        hacker_env_dock_->setVisible(true);
    } else {
        if (hacker_env_dock_) hacker_env_dock_->setVisible(false);
    }

    // --- Emulator Core Manager (always available, scans once) ---
    if (!core_manager_) {
        core_manager_ = new EmulatorCoreManager(this);
        core_manager_->ScanCores();
    }

    // --- Debug panels (Hacker mode only, and not during emulation) ---
    const bool show_debug = (mode == AppMode::Hacker) && !emulation_running;
    const bool show_partial_debug = false;

    if (microProfileDialog)
        microProfileDialog->setVisible(show_debug);
    if (waitTreeWidget)
        waitTreeWidget->setVisible(show_debug);
    if (controller_dialog)
        controller_dialog->setVisible(show_debug);

    // Persist the active mode so it can be queried elsewhere
    ModeSelector::SaveMode(mode);

    LOG_INFO(Frontend, "Applied {} mode layout",
             mode == AppMode::Gamer       ? "Gamer"
             : mode == AppMode::Programmer ? "Programmer"
                                           : "Hacker");
}

void GMainWindow::OnAbout() {
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
}

void GMainWindow::OnExportGame() {
    GameExportDialog dialog(this);

    if (game_list) {
        QVector<GameExportDialog::LibraryEntry> library_entries;
        auto* model = game_list->GetModel();
        if (model) {
            constexpr int kTitleRole = Qt::UserRole + 3;
            constexpr int kPathRole = Qt::UserRole + 4;
            constexpr int kProgramIdRole = Qt::UserRole + 2;

            std::function<void(const QModelIndex&)> collect = [&](const QModelIndex& parent) {
                const int rows = model->rowCount(parent);
                for (int row = 0; row < rows; ++row) {
                    const QModelIndex idx = model->index(row, 0, parent);
                    const QString game_path = idx.data(kPathRole).toString();
                    const QFileInfo game_info(game_path);
                    if (!game_path.isEmpty() && !game_path.startsWith(QStringLiteral("owned://")) &&
                        game_info.exists() && game_info.isFile()) {
                        const QString title = idx.data(kTitleRole).toString().trimmed().isEmpty()
                                                  ? idx.data(Qt::DisplayRole).toString()
                                                  : idx.data(kTitleRole).toString();
                        library_entries.push_back(
                            {title, game_path, idx.data(kProgramIdRole).toULongLong()});
                    }
                    if (model->hasChildren(idx)) {
                        collect(idx);
                    }
                }
            };

            collect(QModelIndex());
        }
        dialog.SetLibraryEntries(std::move(library_entries));
    }

    if (game_list) {
        const QString selected_path = game_list->GetSelectedGamePath();
        const u64 selected_program_id = game_list->GetSelectedProgramId();
        if (!selected_path.isEmpty()) {
            dialog.SetRomPath(selected_path, selected_program_id);
        }
    }
    dialog.exec();
}

void GMainWindow::OnNintendoAccount() {
    NintendoAccountDialog dialog(this);
    connect(&dialog, &NintendoAccountDialog::OwnedLibraryUpdated, this, [this](int) {
        if (game_list) {
            game_list->PopulateAsync(UISettings::values.game_dirs);
        }
    });
    connect(&dialog, &NintendoAccountDialog::AccountUnlinked, this, [this]() {
        if (game_list) {
            game_list->PopulateAsync(UISettings::values.game_dirs);
        }
    });
    dialog.exec();
}

void GMainWindow::OnSteamIntegration() {
    SteamIntegration steam(this);
    const bool installed = steam.IsSteamInstalled();
    const auto shortcuts = steam.ListShortcuts();

    QString message = installed ? tr("Steam is installed.\n") : tr("Steam is not detected.\n");
    message += tr("%1 game shortcut(s) currently managed.\n").arg(shortcuts.size());
    message += tr("Steam is detected by standard install paths, Steam registry settings, or the STEAM_PATH environment variable.\n");
    message += tr("Artwork is fetched from the Steam Store public search endpoint with no API key required.\n");
    message += tr("Add to Steam will still work without custom artwork if a matched store image cannot be found.");

    QMessageBox::information(this, tr("Steam Integration"), message);
}

void GMainWindow::OnShowGameOverlay() {
    if (!emulation_running || !render_window || !render_window->isVisible()) {
        QMessageBox::information(this, tr("Game Overlay"),
                                 tr("Start a game first to open the in-game overlay."));
        return;
    }

    const QString game_name = current_game_path.isEmpty()
                                  ? tr("Running Game")
                                  : QFileInfo(current_game_path).completeBaseName();

    const bool steam_installed = SteamIntegration(this).IsSteamInstalled();
    const QString steam_note = steam_installed
                                   ? tr("Steam overlay is still available with Shift+Tab.")
                                   : tr("Steam integration will not interfere with this overlay.");

    const QString body = tr(
        "<div style='color:white;'>"
        "<p style='font-size:28pt; font-weight:bold; margin:0 0 18px;'>%1</p>"
        "<p style='font-size:12pt; color:#bbbbbb; margin:0 0 22px;'>"
        "A quick overlay for resume, game settings, and instant status."
        "</p>"
        "<p style='font-size:11pt; color:#d0d0d0; margin:0 0 14px;'>"
        "Press <b>F1</b> to open this overlay while a game is running."
        "</p>"
        "<p style='font-size:10pt; color:#909090; margin-top:20px;'>%2</p>"
        "</div>")
        .arg(game_name, steam_note);

    OverlayDialog overlay(render_window, *system, tr("Game Overlay"), body,
                          tr("Resume"), tr("Settings"), Qt::AlignLeft, true);
    if (overlay.exec() == QDialog::Accepted) {
        OnConfigurePerGame();
    }
}

static QString GetSteamArtworkType() {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    return settings.value(QStringLiteral("steam/artwork_type"), QStringLiteral("grids")).toString();
}

static void SetSteamArtworkType(const QString& artwork_type) {
    QSettings settings(QStringLiteral("suyu"), QStringLiteral("SuyuEclipse"));
    settings.setValue(QStringLiteral("steam/artwork_type"), artwork_type);
}

static QString GetSteamArtworkCacheDir() {
    return QString::fromStdString(Common::FS::PathToUTF8String(
        Common::FS::GetSuyuPath(Common::FS::SuyuPath::CacheDir) / "steam_artwork"));
}

void GMainWindow::OnGameListCreateSteamShortcut(u64 program_id, const std::string& game_path) {
    SteamIntegration* steam = new SteamIntegration(this);
    if (!steam->IsSteamInstalled()) {
        QMessageBox::warning(this, tr("Steam Integration"),
                             tr("Unable to find Steam installation. Please install Steam and try again."));
        steam->deleteLater();
        return;
    }

    // Get title from game file
    const FileSys::PatchManager pm{program_id, system->GetFileSystemController(),
                                   system->GetContentProvider()};
    const auto control = pm.GetControlMetadata();
    const auto loader = Loader::GetLoader(*system, vfs->OpenFile(game_path, FileSys::OpenMode::Read));
    std::string game_title = fmt::format("{:016X}", program_id);
    if (control.first != nullptr) {
        game_title = control.first->GetApplicationName();
    } else {
        loader->ReadTitle(game_title);
    }
    const QString qt_game_title = QString::fromStdString(game_title);

    const auto shortcuts = steam->ListShortcuts();
    for (const auto& sc : shortcuts) {
        if (sc.app_name == qt_game_title) {
            QMessageBox::information(this, tr("Steam Integration"),
                                     tr("%1 is already added to Steam.").arg(qt_game_title));
            steam->deleteLater();
            return;
        }
    }

    const QString artwork_type = GetSteamArtworkType();
    const QStringList artwork_labels = {tr("Grid"), tr("Hero"), tr("Icon"), tr("Artwork")};
    const QStringList artwork_values = {QStringLiteral("grids"), QStringLiteral("heroes"),
                                        QStringLiteral("icons"), QStringLiteral("artworks")};
    int default_index = artwork_values.indexOf(artwork_type);
    if (default_index < 0) {
        default_index = 0;
    }

    bool type_ok = false;
    const QString selected_artwork_label = QInputDialog::getItem(
        this, tr("Steam Store Artwork Type"),
        tr("Choose the artwork style to fetch from the Steam Store:"),
        artwork_labels, default_index, false, &type_ok);

    QString selected_artwork_type = artwork_type;
    if (type_ok && !selected_artwork_label.isEmpty()) {
        const int selected_index = artwork_labels.indexOf(selected_artwork_label);
        if (selected_index >= 0 && selected_index < artwork_values.size()) {
            selected_artwork_type = artwork_values[selected_index];
            SetSteamArtworkType(selected_artwork_type);
        }
    }

    const QString cache_dir = GetSteamArtworkCacheDir();
    QDir().mkpath(cache_dir);
    const QString steam_icon_path = QDir(cache_dir).filePath(
        QStringLiteral("%1_%2.png").arg(program_id).arg(selected_artwork_type));
    if (QFileInfo::exists(steam_icon_path)) {
        if (steam->AddGameShortcut(qt_game_title, QString::fromStdString(game_path),
                                   steam_icon_path)) {
            QMessageBox::information(
                this, tr("Steam Integration"),
                tr("%1 has been added to Steam using cached %2 artwork.")
                    .arg(qt_game_title,
                         selected_artwork_label.isEmpty() ? tr("store")
                                                          : selected_artwork_label.toLower()));
        } else {
            QMessageBox::warning(this, tr("Steam Integration"),
                                 tr("Failed to add %1 to Steam using cached artwork.")
                                     .arg(qt_game_title));
        }
        steam->deleteLater();
        return;
    }

    connect(steam, &SteamIntegration::ArtworkFetched, this,
            [this, steam, qt_game_title, game_path](const QString& title, const QString& path) {
                Q_UNUSED(title);
                if (steam->AddGameShortcut(qt_game_title, QString::fromStdString(game_path), path)) {
                    QMessageBox::information(this, this->tr("Steam Integration"),
                                             this->tr("%1 has been added to Steam with artwork.").arg(qt_game_title));
                } else {
                    QMessageBox::warning(this, this->tr("Steam Integration"),
                                         this->tr("Failed to add %1 to Steam after artwork download.").arg(qt_game_title));
                }
                steam->deleteLater();
            });
    connect(steam, &SteamIntegration::ArtworkFetchFailed, this,
            [this, steam, qt_game_title, game_path](const QString& title, const QString& error) {
                Q_UNUSED(title);
                if (steam->AddGameShortcut(qt_game_title, QString::fromStdString(game_path))) {
                    QMessageBox::warning(
                        this, this->tr("Steam Integration"),
                        this->tr("%1 has been added to Steam, but artwork download failed: %2").arg(qt_game_title, error));
                } else {
                    QMessageBox::warning(this, this->tr("Steam Integration"),
                                         this->tr("Failed to add %1 to Steam: %2").arg(qt_game_title, error));
                }
                steam->deleteLater();
            });
    SteamIntegration::ArtworkType artwork_enum = SteamIntegration::ArtworkType::Grid;
    if (selected_artwork_type == QLatin1String("heroes")) {
        artwork_enum = SteamIntegration::ArtworkType::Hero;
    } else if (selected_artwork_type == QLatin1String("icons")) {
        artwork_enum = SteamIntegration::ArtworkType::Icon;
    } else if (selected_artwork_type == QLatin1String("artworks")) {
        artwork_enum = SteamIntegration::ArtworkType::Artwork;
    }
    steam->FetchArtwork(qt_game_title, steam_icon_path, artwork_enum);
    const QString title = tr("Steam Integration");
    QMessageBox::information(this, title,
                             tr("Searching the Steam Store for %1 artwork. Shortcut will be created when the download completes.").arg(qt_game_title));
}

void GMainWindow::OnOpenUserManual() {
    if (!user_manual_widget_) {
        user_manual_widget_ = new UserManualWidget(this);
    }
    user_manual_widget_->show();
    user_manual_widget_->raise();
    user_manual_widget_->activateWindow();
}

void GMainWindow::OnToggleFilterBar() {
    game_list->SetFilterVisible(ui->action_Show_Filter_Bar->isChecked());
    if (ui->action_Show_Filter_Bar->isChecked()) {
        game_list->SetFilterFocus();
    } else {
        game_list->ClearFilter();
    }
}

void GMainWindow::OnToggleStatusBar() {
    statusBar()->setVisible(ui->action_Show_Status_Bar->isChecked());
}

void GMainWindow::OnToggleFoldersInList() {
    UISettings::values.show_folders_in_list = ui->action_Show_Folders_In_List->isChecked();

    game_list->ClearList();
    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);
}

void GMainWindow::OnAlbum() {
    constexpr u64 AlbumId = static_cast<u64>(Service::AM::AppletProgramId::PhotoViewer);
    auto bis_system = system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QMessageBox::warning(this, tr("No firmware available"),
                             tr("Please install the firmware to use the Album applet."));
        return;
    }

    auto album_nca = bis_system->GetEntry(AlbumId, FileSys::ContentRecordType::Program);
    if (!album_nca) {
        QMessageBox::warning(this, tr("Album Applet"),
                             tr("Album applet is not available. Please reinstall firmware."));
        return;
    }

    system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::PhotoViewer);

    const auto filename = QString::fromStdString(album_nca->GetFullPath());
    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename, LibraryAppletParameters(AlbumId, Service::AM::AppletId::PhotoViewer));
}

void GMainWindow::OnCabinet(Service::NFP::CabinetMode mode) {
    constexpr u64 CabinetId = static_cast<u64>(Service::AM::AppletProgramId::Cabinet);
    auto bis_system = system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QMessageBox::warning(this, tr("No firmware available"),
                             tr("Please install the firmware to use the Cabinet applet."));
        return;
    }

    auto cabinet_nca = bis_system->GetEntry(CabinetId, FileSys::ContentRecordType::Program);
    if (!cabinet_nca) {
        QMessageBox::warning(this, tr("Cabinet Applet"),
                             tr("Cabinet applet is not available. Please reinstall firmware."));
        return;
    }

    system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::Cabinet);
    system->GetFrontendAppletHolder().SetCabinetMode(mode);

    const auto filename = QString::fromStdString(cabinet_nca->GetFullPath());
    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename, LibraryAppletParameters(CabinetId, Service::AM::AppletId::Cabinet));
}

void GMainWindow::OnMiiEdit() {
    constexpr u64 MiiEditId = static_cast<u64>(Service::AM::AppletProgramId::MiiEdit);
    auto bis_system = system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QMessageBox::warning(this, tr("No firmware available"),
                             tr("Please install the firmware to use the Mii editor."));
        return;
    }

    auto mii_applet_nca = bis_system->GetEntry(MiiEditId, FileSys::ContentRecordType::Program);
    if (!mii_applet_nca) {
        QMessageBox::warning(this, tr("Mii Edit Applet"),
                             tr("Mii editor is not available. Please reinstall firmware."));
        return;
    }

    system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::MiiEdit);

    const auto filename = QString::fromStdString((mii_applet_nca->GetFullPath()));
    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename, LibraryAppletParameters(MiiEditId, Service::AM::AppletId::MiiEdit));
}

void GMainWindow::OnOpenControllerMenu() {
    constexpr u64 ControllerAppletId = static_cast<u64>(Service::AM::AppletProgramId::Controller);
    auto bis_system = system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QMessageBox::warning(this, tr("No firmware available"),
                             tr("Please install the firmware to use the Controller Menu."));
        return;
    }

    auto controller_applet_nca =
        bis_system->GetEntry(ControllerAppletId, FileSys::ContentRecordType::Program);
    if (!controller_applet_nca) {
        QMessageBox::warning(this, tr("Controller Applet"),
                             tr("Controller Menu is not available. Please reinstall firmware."));
        return;
    }

    system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::Controller);

    const auto filename = QString::fromStdString((controller_applet_nca->GetFullPath()));
    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename,
             LibraryAppletParameters(ControllerAppletId, Service::AM::AppletId::Controller));
}

void GMainWindow::OnHomeMenu() {
    constexpr u64 QLaunchId = static_cast<u64>(Service::AM::AppletProgramId::QLaunch);
    auto bis_system = system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        QMessageBox::warning(this, tr("No firmware available"),
                             tr("Please install the firmware to use the Home Menu."));
        return;
    }

    auto qlaunch_applet_nca = bis_system->GetEntry(QLaunchId, FileSys::ContentRecordType::Program);
    if (!qlaunch_applet_nca) {
        QMessageBox::warning(this, tr("Home Menu Applet"),
                             tr("Home Menu is not available. Please reinstall firmware."));
        return;
    }

    system->GetFrontendAppletHolder().SetCurrentAppletId(Service::AM::AppletId::QLaunch);

    const auto filename = QString::fromStdString((qlaunch_applet_nca->GetFullPath()));
    UISettings::values.roms_path = QFileInfo(filename).path().toStdString();
    BootGame(filename, LibraryAppletParameters(QLaunchId, Service::AM::AppletId::QLaunch));
}

void GMainWindow::OnCaptureScreenshot() {
    if (emu_thread == nullptr || !emu_thread->IsRunning()) {
        return;
    }

    const u64 title_id = system->GetApplicationProcessProgramID();
    const auto screenshot_path =
        QString::fromStdString(Common::FS::GetSuyuPathString(Common::FS::SuyuPath::ScreenshotsDir));
    const auto date =
        QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_hh-mm-ss-zzz"));
    QString filename = QStringLiteral("%1/%2_%3.png")
                           .arg(screenshot_path)
                           .arg(title_id, 16, 16, QLatin1Char{'0'})
                           .arg(date);

    if (!Common::FS::CreateDir(screenshot_path.toStdString())) {
        return;
    }

#ifdef _WIN32
    if (UISettings::values.enable_screenshot_save_as) {
        OnPauseGame();
        filename = QFileDialog::getSaveFileName(this, tr("Capture Screenshot"), filename,
                                                tr("PNG Image (*.png)"));
        OnStartGame();
        if (filename.isEmpty()) {
            return;
        }
    }
#endif
    render_window->CaptureScreenshot(filename);
}

// TODO: Written 2020-10-01: Remove per-game config migration code when it is irrelevant
void GMainWindow::MigrateConfigFiles() {
    const auto config_dir_fs_path = Common::FS::GetSuyuPath(Common::FS::SuyuPath::ConfigDir);
    const QDir config_dir =
        QDir(QString::fromStdString(Common::FS::PathToUTF8String(config_dir_fs_path)));
    const QStringList config_dir_list = config_dir.entryList(QStringList(QStringLiteral("*.ini")));

    if (!Common::FS::CreateDirs(config_dir_fs_path / "custom")) {
        LOG_ERROR(Frontend, "Failed to create new config file directory");
    }

    for (auto it = config_dir_list.constBegin(); it != config_dir_list.constEnd(); ++it) {
        const auto filename = it->toStdString();
        if (filename.find_first_not_of("0123456789abcdefACBDEF", 0) < 16) {
            continue;
        }
        const auto origin = config_dir_fs_path / filename;
        const auto destination = config_dir_fs_path / "custom" / filename;
        LOG_INFO(Frontend, "Migrating config file from {} to {}", origin.string(),
                 destination.string());
        if (!Common::FS::RenameFile(origin, destination)) {
            // Delete the old config file if one already exists in the new location.
            Common::FS::RemoveFile(origin);
        }
    }
}

void GMainWindow::UpdateWindowTitle(std::string_view title_name, std::string_view title_version,
                                    std::string_view gpu_vendor) {
    const auto build_fullname = std::string(Common::g_build_fullname);
    const auto branch_name = std::string(Common::g_scm_branch);
    const auto description = std::string(Common::g_scm_desc);
    const auto build_id = std::string(Common::g_build_id);

    const auto suyu_title = fmt::format("suyu | {}-{}", branch_name, description);
    const auto override_title =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto window_title = !build_fullname.empty() ? build_fullname
                                                      : (override_title.empty() ? suyu_title : override_title);

    if (title_name.empty()) {
        setWindowTitle(QString::fromStdString(window_title));
    } else {
        const auto run_title = [window_title, title_name, title_version, gpu_vendor]() {
            if (title_version.empty()) {
                return fmt::format("{} | {} | {}", window_title, title_name, gpu_vendor);
            }
            return fmt::format("{} | {} | {} | {}", window_title, title_name, title_version,
                               gpu_vendor);
        }();
        setWindowTitle(QString::fromStdString(run_title));
    }
}

std::string GMainWindow::CreateTASFramesString(
    std::array<size_t, InputCommon::TasInput::PLAYER_NUMBER> frames) const {
    std::string string = "";
    size_t maxPlayerIndex = 0;
    for (size_t i = 0; i < frames.size(); i++) {
        if (frames[i] != 0) {
            if (maxPlayerIndex != 0)
                string += ", ";
            while (maxPlayerIndex++ != i)
                string += "0, ";
            string += std::to_string(frames[i]);
        }
    }
    return string;
}

QString GMainWindow::GetTasStateDescription() const {
    auto [tas_status, current_tas_frame, total_tas_frames] = input_subsystem->GetTas()->GetStatus();
    std::string tas_frames_string = CreateTASFramesString(total_tas_frames);
    switch (tas_status) {
    case InputCommon::TasInput::TasState::Running:
        return tr("TAS state: Running %1/%2")
            .arg(current_tas_frame)
            .arg(QString::fromStdString(tas_frames_string));
    case InputCommon::TasInput::TasState::Recording:
        return tr("TAS state: Recording %1").arg(total_tas_frames[0]);
    case InputCommon::TasInput::TasState::Stopped:
        return tr("TAS state: Idle %1/%2")
            .arg(current_tas_frame)
            .arg(QString::fromStdString(tas_frames_string));
    default:
        return tr("TAS State: Invalid");
    }
}

void GMainWindow::OnTasStateChanged() {
    bool is_running = false;
    bool is_recording = false;
    if (emulation_running) {
        const InputCommon::TasInput::TasState tas_status =
            std::get<0>(input_subsystem->GetTas()->GetStatus());
        is_running = tas_status == InputCommon::TasInput::TasState::Running;
        is_recording = tas_status == InputCommon::TasInput::TasState::Recording;
    }

    ui->action_TAS_Start->setText(is_running ? tr("&Stop Running") : tr("&Start"));
    ui->action_TAS_Record->setText(is_recording ? tr("Stop R&ecording") : tr("R&ecord"));

    ui->action_TAS_Start->setEnabled(emulation_running);
    ui->action_TAS_Record->setEnabled(emulation_running);
    ui->action_TAS_Reset->setEnabled(emulation_running);
}

void GMainWindow::UpdateStatusBar() {
    if (emu_thread == nullptr || !system->IsPoweredOn()) {
        status_bar_update_timer.stop();
        return;
    }

    if (Settings::values.tas_enable) {
        tas_label->setText(GetTasStateDescription());
    } else {
        tas_label->clear();
    }

    auto results = system->GetAndResetPerfStats();
    auto& shader_notify = system->GPU().ShaderNotify();
    const int shaders_building = shader_notify.ShadersBuilding();

    if (shaders_building > 0) {
        shader_building_label->setText(tr("Building: %n shader(s)", "", shaders_building));
        shader_building_label->setVisible(true);
    } else {
        shader_building_label->setVisible(false);
    }

    const auto res_info = Settings::values.resolution_info;
    const auto res_scale = res_info.up_factor;
    res_scale_label->setText(
        tr("Scale: %1x", "%1 is the resolution scaling factor").arg(res_scale));

    if (Settings::values.use_speed_limit.GetValue()) {
        emu_speed_label->setText(tr("Speed: %1% / %2%")
                                     .arg(results.emulation_speed * 100.0, 0, 'f', 0)
                                     .arg(Settings::values.speed_limit.GetValue()));
    } else {
        emu_speed_label->setText(tr("Speed: %1%").arg(results.emulation_speed * 100.0, 0, 'f', 0));
    }
    if (!Settings::values.use_speed_limit) {
        game_fps_label->setText(
            tr("Game: %1 FPS (Unlocked)").arg(std::round(results.average_game_fps), 0, 'f', 0));
    } else {
        game_fps_label->setText(
            tr("Game: %1 FPS").arg(std::round(results.average_game_fps), 0, 'f', 0));
    }
    emu_frametime_label->setText(tr("Frame: %1 ms").arg(results.frametime * 1000.0, 0, 'f', 2));

    res_scale_label->setVisible(true);
    emu_speed_label->setVisible(!Settings::values.use_multi_core.GetValue());
    game_fps_label->setVisible(true);
    emu_frametime_label->setVisible(true);
    firmware_label->setVisible(false);
}

void GMainWindow::UpdateGPUAccuracyButton() {
    const auto gpu_accuracy = Settings::values.gpu_accuracy.GetValue();
    const auto gpu_accuracy_text =
        ConfigurationShared::gpu_accuracy_texts_map.find(gpu_accuracy)->second;
    gpu_accuracy_button->setText(gpu_accuracy_text.toUpper());
    gpu_accuracy_button->setChecked(gpu_accuracy != Settings::GpuAccuracy::Low);
}

void GMainWindow::UpdateDockedButton() {
    const auto console_mode = Settings::values.use_docked_mode.GetValue();
    dock_status_button->setChecked(Settings::IsDockedMode());
    dock_status_button->setText(
        ConfigurationShared::use_docked_mode_texts_map.find(console_mode)->second.toUpper());
}

void GMainWindow::UpdateAPIText() {
    const auto api = Settings::values.renderer_backend.GetValue();
    const auto renderer_status_text =
        ConfigurationShared::renderer_backend_texts_map.find(api)->second;
    renderer_status_button->setText(renderer_status_text.toUpper());
}

void GMainWindow::UpdateFilterText() {
    const auto filter = Settings::values.scaling_filter.GetValue();
    const auto filter_text = ConfigurationShared::scaling_filter_texts_map.find(filter)->second;
    filter_status_button->setText(filter == Settings::ScalingFilter::Fsr ? tr("FSR")
                                                                         : filter_text.toUpper());
}

void GMainWindow::UpdateAAText() {
    const auto aa_mode = Settings::values.anti_aliasing.GetValue();
    const auto aa_text = ConfigurationShared::anti_aliasing_texts_map.find(aa_mode)->second;
    aa_status_button->setText(aa_mode == Settings::AntiAliasing::None
                                  ? QStringLiteral(QT_TRANSLATE_NOOP("GMainWindow", "NO AA"))
                                  : aa_text.toUpper());
}

void GMainWindow::UpdateVolumeUI() {
    const auto volume_value = static_cast<int>(Settings::values.volume.GetValue());
    volume_slider->setValue(volume_value);
    if (Settings::values.audio_muted) {
        volume_button->setChecked(false);
        volume_button->setText(tr("VOLUME: MUTE"));
    } else {
        volume_button->setChecked(true);
        volume_button->setText(tr("VOLUME: %1%", "Volume percentage (e.g. 50%)").arg(volume_value));
    }
}

void GMainWindow::UpdateStatusButtons() {
    renderer_status_button->setChecked(Settings::values.renderer_backend.GetValue() ==
                                       Settings::RendererBackend::Vulkan);
    UpdateAPIText();
    UpdateGPUAccuracyButton();
    UpdateDockedButton();
    UpdateFilterText();
    UpdateAAText();
    UpdateVolumeUI();
}

void GMainWindow::UpdateUISettings() {
    if (!ui->action_Fullscreen->isChecked()) {
        UISettings::values.geometry = saveGeometry();
        UISettings::values.renderwindow_geometry = render_window->saveGeometry();
    }
    UISettings::values.state = saveState();
#if MICROPROFILE_ENABLED
    UISettings::values.microprofile_geometry = microProfileDialog->saveGeometry();
    UISettings::values.microprofile_visible = microProfileDialog->isVisible();
#endif
    UISettings::values.single_window_mode = ui->action_Single_Window_Mode->isChecked();
    UISettings::values.fullscreen = ui->action_Fullscreen->isChecked();
    UISettings::values.display_titlebar = ui->action_Display_Dock_Widget_Headers->isChecked();
    UISettings::values.show_filter_bar = ui->action_Show_Filter_Bar->isChecked();
    UISettings::values.show_status_bar = ui->action_Show_Status_Bar->isChecked();
    UISettings::values.show_folders_in_list = ui->action_Show_Folders_In_List->isChecked();
    UISettings::values.first_start = false;
}

void GMainWindow::UpdateInputDrivers() {
    if (!input_subsystem) {
        return;
    }
    input_subsystem->PumpEvents();
}

void GMainWindow::HideMouseCursor() {
    if (emu_thread == nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.stop();
        ShowMouseCursor();
        return;
    }
    render_window->setCursor(QCursor(Qt::BlankCursor));
}

void GMainWindow::ShowMouseCursor() {
    render_window->unsetCursor();
    if (emu_thread != nullptr && UISettings::values.hide_mouse) {
        mouse_hide_timer.start();
    }
}

void GMainWindow::OnMouseActivity() {
    if (!Settings::values.mouse_panning) {
        ShowMouseCursor();
    }
}

void GMainWindow::OnCheckFirmwareDecryption() {
    if (!ContentManager::AreKeysPresent()) {
        LOG_INFO(Frontend, "No local decryption keys detected.");
    }

    SetFirmwareVersion();
    UpdateMenuState();
}

bool GMainWindow::CheckFirmwarePresence() {
    auto bis_system = system->GetFileSystemController().GetSystemNANDContents();
    if (!bis_system) {
        return false;
    }

    // Accept firmware as valid if ANY known system applet NCA is present.
    // OR logic here avoids false negatives when only some firmware files were installed.
    const auto has_nca = [&](Service::AM::AppletProgramId id) {
        return bis_system->GetEntry(static_cast<u64>(id),
                                    FileSys::ContentRecordType::Program) != nullptr;
    };
    return has_nca(Service::AM::AppletProgramId::QLaunch) ||
           has_nca(Service::AM::AppletProgramId::MiiEdit) ||
           has_nca(Service::AM::AppletProgramId::SoftwareKeyboard) ||
           has_nca(Service::AM::AppletProgramId::ProfileSelect);
}

void GMainWindow::SetFirmwareVersion() {
    Service::Set::FirmwareVersionFormat firmware_data{};
    const auto result = Service::Set::GetFirmwareVersionImpl(
        firmware_data, *system, Service::Set::GetFirmwareVersionType::Version2);

    if (result.IsError() || !CheckFirmwarePresence()) {
        LOG_INFO(Frontend, "Installed firmware: No firmware available");
        firmware_label->setVisible(false);
        return;
    }

    firmware_label->setVisible(true);

    const std::string display_version(firmware_data.display_version.data());
    const std::string display_title(firmware_data.display_title.data());

    LOG_INFO(Frontend, "Installed firmware: {}", display_title);

    firmware_label->setText(QString::fromStdString(display_version));
    firmware_label->setToolTip(QString::fromStdString(display_title));
}

bool GMainWindow::SelectRomFSDumpTarget(const FileSys::ContentProvider& installed, u64 program_id,
                                        u64* selected_title_id, u8* selected_content_record_type) {
    using ContentInfo = std::tuple<u64, FileSys::TitleType, FileSys::ContentRecordType>;
    boost::container::flat_set<ContentInfo> available_title_ids;

    const auto RetrieveEntries = [&](FileSys::TitleType title_type,
                                     FileSys::ContentRecordType record_type) {
        const auto entries = installed.ListEntriesFilter(title_type, record_type);
        for (const auto& entry : entries) {
            if (FileSys::GetBaseTitleID(entry.title_id) == program_id &&
                installed.GetEntry(entry)->GetStatus() == Loader::ResultStatus::Success) {
                available_title_ids.insert({entry.title_id, title_type, record_type});
            }
        }
    };

    RetrieveEntries(FileSys::TitleType::Application, FileSys::ContentRecordType::Program);
    RetrieveEntries(FileSys::TitleType::Application, FileSys::ContentRecordType::HtmlDocument);
    RetrieveEntries(FileSys::TitleType::Application, FileSys::ContentRecordType::LegalInformation);
    RetrieveEntries(FileSys::TitleType::AOC, FileSys::ContentRecordType::Data);

    if (available_title_ids.empty()) {
        return false;
    }

    size_t title_index = 0;

    if (available_title_ids.size() > 1) {
        QStringList list;
        for (auto& [title_id, title_type, record_type] : available_title_ids) {
            const auto hex_title_id = QString::fromStdString(fmt::format("{:X}", title_id));
            if (record_type == FileSys::ContentRecordType::Program) {
                list.push_back(QStringLiteral("Program [%1]").arg(hex_title_id));
            } else if (record_type == FileSys::ContentRecordType::HtmlDocument) {
                list.push_back(QStringLiteral("HTML document [%1]").arg(hex_title_id));
            } else if (record_type == FileSys::ContentRecordType::LegalInformation) {
                list.push_back(QStringLiteral("Legal information [%1]").arg(hex_title_id));
            } else {
                list.push_back(
                    QStringLiteral("DLC %1 [%2]").arg(title_id & 0x7FF).arg(hex_title_id));
            }
        }

        bool ok;
        const auto res = QInputDialog::getItem(
            this, tr("Select RomFS Dump Target"),
            tr("Please select which RomFS you would like to dump."), list, 0, false, &ok);
        if (!ok) {
            return false;
        }

        title_index = list.indexOf(res);
    }

    const auto& [title_id, title_type, record_type] = *available_title_ids.nth(title_index);
    *selected_title_id = title_id;
    *selected_content_record_type = static_cast<u8>(record_type);
    return true;
}

bool GMainWindow::ConfirmClose() {
    if (emu_thread == nullptr ||
        UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Never) {
        return true;
    }
    if (!system->GetExitLocked() &&
        UISettings::values.confirm_before_stopping.GetValue() == ConfirmStop::Ask_Based_On_Game) {
        return true;
    }
    const auto text = tr("Are you sure you want to close suyu?");
    return question(this, tr("suyu"), text);
}

void GMainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UpdateUISettings();
    game_list->SaveInterfaceLayout();
    UISettings::SaveWindowState();
    hotkey_registry.SaveHotkeys();

    // Unload controllers early
    controller_dialog->UnloadController();
    game_list->UnloadController();

    // Shutdown session if the emu thread is active...
    if (emu_thread != nullptr) {
        ShutdownGame();
    }

    render_window->close();
    multiplayer_state->Close();
    system->HIDCore().UnloadInputDevices();
    system->GetRoomNetwork().Shutdown();

    QWidget::closeEvent(event);
}

static bool IsSingleFileDropEvent(const QMimeData* mime) {
    return mime->hasUrls() && mime->urls().length() == 1;
}

void GMainWindow::AcceptDropEvent(QDropEvent* event) {
    if (IsSingleFileDropEvent(event->mimeData())) {
        event->setDropAction(Qt::DropAction::LinkAction);
        event->accept();
    }
}

bool GMainWindow::DropAction(QDropEvent* event) {
    if (!IsSingleFileDropEvent(event->mimeData())) {
        return false;
    }

    const QMimeData* mime_data = event->mimeData();
    const QString& filename = mime_data->urls().at(0).toLocalFile();

    if (emulation_running && QFileInfo(filename).suffix() == QStringLiteral("bin")) {
        // Amiibo
        LoadAmiibo(filename);
    } else {
        // Game
        if (ConfirmChangeGame()) {
            BootGame(filename, ApplicationAppletParameters());
        }
    }
    return true;
}

void GMainWindow::dropEvent(QDropEvent* event) {
    DropAction(event);
}

void GMainWindow::dragEnterEvent(QDragEnterEvent* event) {
    AcceptDropEvent(event);
}

void GMainWindow::dragMoveEvent(QDragMoveEvent* event) {
    AcceptDropEvent(event);
}

bool GMainWindow::ConfirmChangeGame() {
    if (emu_thread == nullptr)
        return true;

    // Use custom question to link controller navigation
    return question(
        this, tr("suyu"),
        tr("Are you sure you want to stop the emulation? Any unsaved progress will be lost."),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
}

bool GMainWindow::ConfirmForceLockedExit() {
    if (emu_thread == nullptr) {
        return true;
    }
    const auto text = tr("The currently running application has requested suyu to not exit.\n\n"
                         "Would you like to bypass this and exit anyway?");

    return question(this, tr("suyu"), text);
}

void GMainWindow::RequestGameExit() {
    if (!system->IsPoweredOn()) {
        return;
    }

    system->SetExitRequested(true);
    system->GetAppletManager().RequestExit();
}

void GMainWindow::filterBarSetChecked(bool state) {
    ui->action_Show_Filter_Bar->setChecked(state);
    emit(OnToggleFilterBar());
}

void GMainWindow::UpdateUITheme() {
    const QString default_theme = QString::fromStdString(UISettings::default_theme.data());
    QString current_theme = UISettings::values.theme;
    if (current_theme.isEmpty()) {
        current_theme = default_theme;
    }

    UpdateIcons(current_theme);

    /* Find the stylesheet to load */
    if (TryLoadStylesheet(current_theme)) {
        return;
    }

    // Reading new theme failed, loading default stylesheet
    LOG_ERROR(Frontend, "Unable to open style \"{}\", fallback to the default theme",
              current_theme.toStdString());

    if (TryLoadStylesheet(QStringLiteral(":/%1").arg(default_theme))) {
        return;
    }

    // Reading default failed, loading empty stylesheet
    LOG_ERROR(Frontend, "Unable to set default style, stylesheet file not found");

    qApp->setStyleSheet({});
    setStyleSheet({});
}

void GMainWindow::UpdateIcons(const QString& theme_path) {
    // Get the theme directory from its path
    const QString normalized_theme_path = theme_path.endsWith(QLatin1Char('/'))
        ? theme_path.left(theme_path.size() - 1)
        : theme_path;
    const int last_slash = normalized_theme_path.lastIndexOf(QLatin1Char('/'));
    const QString theme_dir = last_slash >= 0
        ? normalized_theme_path.mid(last_slash + 1)
        : normalized_theme_path;

    // Append _dark to the theme name to use dark variant icons
    if (CheckDarkMode()) {
        LOG_DEBUG(Frontend, "Using icons from: {}", theme_dir.toStdString() + "_dark");
        QIcon::setThemeName(theme_dir + QStringLiteral("_dark"));
    } else {
        LOG_DEBUG(Frontend, "Using icons from: {}", theme_dir.toStdString());
        QIcon::setThemeName(theme_dir);
    }

    const QString theme_directory{
        QString::fromStdString(Common::FS::GetSuyuPathString(Common::FS::SuyuPath::ThemesDir))};

    // Set path for default icons
    // Use icon resources from application binary and current theme local subdirectory, if it exists
    QStringList theme_paths;
    theme_paths << QStringLiteral(":/icons") << theme_directory;
    QIcon::setThemeSearchPaths(theme_paths);

    // Change current directory, to allow user themes to add their own icons
    if (!theme_dir.isEmpty() && !normalized_theme_path.startsWith(QLatin1String(":/"))) {
        QDir::setCurrent(QStringLiteral("%1/%2").arg(theme_directory, theme_dir));
    }

    emit UpdateThemedIcons();
}

bool GMainWindow::TryLoadStylesheet(const QString& theme_uri) {
    LOG_DEBUG(Frontend, "TryLoadStylesheet()");
    QString style_path;

    // Use themed stylesheet if it exists
    if (CheckDarkMode()) {
        style_path = theme_uri + QStringLiteral("/dark.qss");
    } else {
        style_path = theme_uri + QStringLiteral("/light.qss");
    }
    if (!QFile::exists(style_path)) {
        LOG_DEBUG(Frontend, "No themed (light/dark) stylesheet, using default one");
        // Use common stylesheet if themed one does not exist
        style_path = theme_uri + QStringLiteral("/style.qss");
    }

    // Loading stylesheet
    QFile style_file(style_path);
    if (style_file.open(QFile::ReadOnly | QFile::Text)) {
        // Update the color palette before applying the stylesheet
        UpdateThemePalette();

        LOG_DEBUG(Frontend, "Loading stylesheet in: {}", theme_uri.toStdString());
        QTextStream ts_theme(&style_file);
        const QString stylesheet = ts_theme.readAll();
        qApp->setStyleSheet(stylesheet);
        setStyleSheet(stylesheet);
        SetCustomStylesheet();

        return true;
    }
    // Opening the file failed
    return false;
}

bool GMainWindow::TryLoadStylesheet(const std::filesystem::path& theme_path) {
    return TryLoadStylesheet(QString::fromStdString(theme_path.string() + "/"));
}

static void AdjustLinkColor() {
    QPalette new_pal(qApp->palette());
    if (GMainWindow::CheckDarkMode()) {
        new_pal.setColor(QPalette::Link, QColor(0, 190, 255, 255));
    } else {
        new_pal.setColor(QPalette::Link, QColor(0, 140, 200, 255));
    }
    if (qApp->palette().color(QPalette::Link) != new_pal.color(QPalette::Link)) {
        qApp->setPalette(new_pal);
    }
}

void GMainWindow::UpdateThemePalette() {
    LOG_DEBUG(Frontend, "UpdateThemePalette()");
    QPalette themePalette(qApp->palette());
#ifdef _WIN32
    QColor dark(25, 25, 25);
    QString style_name;
    if (CheckDarkMode()) {
        // We check that the dark mode state is "On" and force a dark palette
        if (UISettings::values.dark_mode_state == DarkModeState::On) {
            // Set Default Windows Dark palette on Windows platforms to force Dark mode
            themePalette.setColor(QPalette::Window, Qt::black);
            themePalette.setColor(QPalette::WindowText, Qt::white);
            themePalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
            themePalette.setColor(QPalette::Base, Qt::black);
            themePalette.setColor(QPalette::AlternateBase, dark);
            themePalette.setColor(QPalette::ToolTipBase, Qt::white);
            themePalette.setColor(QPalette::ToolTipText, Qt::black);
            themePalette.setColor(QPalette::Text, Qt::white);
            themePalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
            themePalette.setColor(QPalette::Dark, QColor(128, 128, 128));
            themePalette.setColor(QPalette::Shadow, Qt::white);
            themePalette.setColor(QPalette::Button, Qt::black);
            themePalette.setColor(QPalette::ButtonText, Qt::white);
            themePalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
            themePalette.setColor(QPalette::BrightText, QColor(192, 192, 192));
            themePalette.setColor(QPalette::Link, QColor(0, 140, 200));
            themePalette.setColor(QPalette::Highlight, QColor(24, 70, 93));
            themePalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0, 85, 255));
            themePalette.setColor(QPalette::HighlightedText, QColor(239, 240, 241));
            themePalette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                                  QColor(239, 240, 241));
        }

        // AlternateBase is kept at rgb(233, 231, 227) or rgb(245, 245, 245) on Windows dark
        // palette, fix this. Sometimes, it even is rgb(0, 0, 0), but uses a very light gray for
        // alternate rows, do not know why
        if (themePalette.alternateBase().color() == QColor(233, 231, 227) ||
            themePalette.alternateBase().color() == QColor(245, 245, 245) ||
            themePalette.alternateBase().color() == QColor(0, 0, 0)) {
            themePalette.setColor(QPalette::AlternateBase, dark);
            alternate_base_modified = true;
        }
        // Use fusion theme, since its close to windowsvista, but works well with a dark palette
        style_name = QStringLiteral("fusion");
    } else {
        // Reset AlternateBase if it has been modified
        if (alternate_base_modified) {
            themePalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
            alternate_base_modified = false;
        }
        // Reset light palette
        themePalette = this->style()->standardPalette();
        // Reset Windows theme to the default
        style_name = QStringLiteral("windowsvista");
    }
    LOG_DEBUG(Frontend, "Using style: {}", style_name.toStdString());
    qApp->setStyle(style_name);
#elif defined(__APPLE__)
    // Force the usage of the light palette in light mode
    if (CheckDarkMode()) {
        // Reset dark palette
        themePalette = this->style()->standardPalette();
    } else {
        themePalette.setColor(QPalette::Window, QColor(236, 236, 236));
        themePalette.setColor(QPalette::WindowText, Qt::black);
        themePalette.setColor(QPalette::Disabled, QPalette::WindowText, Qt::black);
        themePalette.setColor(QPalette::Base, Qt::white);
        themePalette.setColor(QPalette::AlternateBase, QColor(245, 245, 245));
        themePalette.setColor(QPalette::ToolTipBase, Qt::white);
        themePalette.setColor(QPalette::ToolTipText, Qt::black);
        themePalette.setColor(QPalette::Text, Qt::black);
        themePalette.setColor(QPalette::Disabled, QPalette::Text, Qt::black);
        themePalette.setColor(QPalette::Dark, QColor(191, 191, 191));
        themePalette.setColor(QPalette::Shadow, Qt::black);
        themePalette.setColor(QPalette::Button, QColor(236, 236, 236));
        themePalette.setColor(QPalette::ButtonText, Qt::black);
        themePalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(147, 147, 147));
        themePalette.setColor(QPalette::BrightText, Qt::white);
        themePalette.setColor(QPalette::Link, QColor(0, 140, 200));
        themePalette.setColor(QPalette::Highlight, QColor(179, 215, 255));
        themePalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(220, 220, 220));
        themePalette.setColor(QPalette::HighlightedText, Qt::black);
        themePalette.setColor(QPalette::Disabled, QPalette::HighlightedText, Qt::black);
    }
#else
    if (CheckDarkMode()) {
        // Set Dark palette on non Windows platforms (that may not have a dark palette)
        themePalette.setColor(QPalette::Window, QColor(53, 53, 53));
        themePalette.setColor(QPalette::WindowText, Qt::white);
        themePalette.setColor(QPalette::Disabled, QPalette::WindowText, QColor(127, 127, 127));
        themePalette.setColor(QPalette::Base, QColor(42, 42, 42));
        themePalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
        themePalette.setColor(QPalette::ToolTipBase, Qt::white);
        themePalette.setColor(QPalette::ToolTipText, QColor(53, 53, 53));
        themePalette.setColor(QPalette::Text, Qt::white);
        themePalette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
        themePalette.setColor(QPalette::Dark, QColor(35, 35, 35));
        themePalette.setColor(QPalette::Shadow, QColor(20, 20, 20));
        themePalette.setColor(QPalette::Button, QColor(53, 53, 53));
        themePalette.setColor(QPalette::ButtonText, Qt::white);
        themePalette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(127, 127, 127));
        themePalette.setColor(QPalette::BrightText, Qt::red);
        themePalette.setColor(QPalette::Link, QColor(42, 130, 218));
        themePalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
        themePalette.setColor(QPalette::Disabled, QPalette::Highlight, QColor(80, 80, 80));
        themePalette.setColor(QPalette::HighlightedText, Qt::white);
        themePalette.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(127, 127, 127));
    } else {
        // Reset light palette
        themePalette = this->style()->standardPalette();
    }
#endif
    qApp->setPalette(themePalette);
    AdjustLinkColor();
}

void GMainWindow::SetCustomStylesheet() {
    setStyleSheet(QStringLiteral("QStatusBar::item { border: none; }"));

    // Set "dark" qss property value, that may be used in stylesheets
    bool is_dark_mode = CheckDarkMode();
    if (renderer_status_button) {
        renderer_status_button->setProperty("dark", is_dark_mode);
    }
    if (gpu_accuracy_button) {
        gpu_accuracy_button->setProperty("dark", is_dark_mode);
    }
#ifdef _WIN32
    // Windows dark mode uses "fusion" style. Make it look like more "windowsvista" light style
    if (is_dark_mode) {
        /* the groove expands to the size of the slider by default. by giving it a height, it has a
        fixed size */
        /* handle is placed by default on the contents rect of the groove. Negative margin expands
        it outside the groove */
        setStyleSheet(QStringLiteral("QSlider:horizontal{ height:30px; }\
 QSlider::sub-page:horizontal { background-color: palette(highlight); }\
 QSlider::add-page:horizontal { background-color: palette(midlight);}\
 QSlider::groove:horizontal { border-width: 1px; margin: 1px 0; height: 2px;}\
 QSlider::handle:horizontal { border-width: 1px; border-style: solid; border-color: palette(dark);\
     width: 10px; margin: -10px 0px; }\
 QSlider::handle { background-color: palette(button); }\
 QSlider::handle:hover { background-color: palette(highlight); }"));
    }
#endif
}

#ifdef __unix__
bool GMainWindow::ListenColorSchemeChange() {
    auto bus = QDBusConnection::sessionBus();
    if (bus.isConnected()) {
        const QString dbus_service = QStringLiteral("org.freedesktop.portal.Desktop");
        const QString dbus_path = QStringLiteral("/org/freedesktop/portal/desktop");
        const QString dbus_interface = QStringLiteral("org.freedesktop.portal.Settings");
        const QString dbus_method = QStringLiteral("SettingChanged");
        QStringList dbus_arguments;
        dbus_arguments << QStringLiteral("org.freedesktop.appearance")
                       << QStringLiteral("color-scheme");
        const QString dbus_signature = QStringLiteral("ssv");

        LOG_INFO(Frontend, "Connected to DBus, listening for OS theme changes");
        return bus.connect(dbus_service, dbus_path, dbus_interface, dbus_method, dbus_arguments,
                           dbus_signature, this, SLOT(UpdateUITheme()));
    }
    LOG_WARNING(Frontend, "Unable to connect to DBus to listen for OS theme changes");
    return false;
}
#endif

bool GMainWindow::CheckDarkMode() {
#ifdef _WIN32
    if (UISettings::values.dark_mode_state == DarkModeState::On) {
        return true;
    }
    if (UISettings::values.dark_mode_state == DarkModeState::Off) {
        return false;
    }

    const bool is_dark_mode_auto = qgetenv("QT_QPA_PLATFORM").contains("darkmode=2");
#else
    const bool is_dark_mode_auto = UISettings::values.dark_mode_state == DarkModeState::Auto;
#endif
    if (!is_dark_mode_auto) {
        return UISettings::values.dark_mode_state == DarkModeState::On;
    } else {
        const QPalette current_palette(qApp->palette());
#ifdef __unix__
        QProcess process;

        // Using the freedesktop specifications for checking dark mode
        LOG_DEBUG(Frontend, "Retrieving theme from freedesktop color-scheme...");
        QStringList gdbus_arguments;
        gdbus_arguments << QStringLiteral("--dest=org.freedesktop.portal.Desktop")
                        << QStringLiteral("--object-path /org/freedesktop/portal/desktop")
                        << QStringLiteral("--method org.freedesktop.portal.Settings.Read")
                        << QStringLiteral("org.freedesktop.appearance color-scheme");
        process.start(QStringLiteral("gdbus call --session"), gdbus_arguments);
        process.waitForFinished(1000);
        QByteArray dbus_output = process.readAllStandardOutput();

        if (!dbus_output.isEmpty()) {
            const int systemColorSchema = QString::fromUtf8(dbus_output).trimmed().right(1).toInt();
            return systemColorSchema == 1;
        }

        // Try alternative for Gnome if the previous one failed
        QStringList gsettings_arguments;
        gsettings_arguments << QStringLiteral("get")
                            << QStringLiteral("org.gnome.desktop.interface")
                            << QStringLiteral("color-scheme");

        LOG_DEBUG(Frontend, "failed, retrieving theme from gsettings color-scheme...");
        process.start(QStringLiteral("gsettings"), gsettings_arguments);
        process.waitForFinished(1000);
        QByteArray gsettings_output = process.readAllStandardOutput();

        // Try older gtk-theme method if the previous one failed
        if (gsettings_output.isEmpty()) {
            LOG_DEBUG(Frontend, "failed, retrieving theme from gtk-theme...");
            gsettings_arguments.takeLast();
            gsettings_arguments << QStringLiteral("gtk-theme");

            process.start(QStringLiteral("gsettings"), gsettings_arguments);
            process.waitForFinished(1000);
            gsettings_output = process.readAllStandardOutput();
        }

        // Interpret gsettings value if it succeeded
        if (!gsettings_output.isEmpty()) {
            QString systeme_theme = QString::fromUtf8(gsettings_output);
            LOG_DEBUG(Frontend, "Gsettings output: {}", systeme_theme.toStdString());
            return systeme_theme.contains(QStringLiteral("dark"), Qt::CaseInsensitive);
        }
        LOG_DEBUG(Frontend, "failed, retrieving theme from palette");
#endif
        // Use default method based on palette swap by OS. It is the only method on Windows with
        // Qt 5. Windows needs QT_QPA_PLATFORM env variable set to windows:darkmode=2 to force
        // palette change
        return (current_palette.color(QPalette::WindowText).lightness() >
                current_palette.color(QPalette::Window).lightness());
    }
}

void GMainWindow::changeEvent(QEvent* event) {
    // PaletteChange event appears to only reach so far into the GUI, explicitly asking to
    // UpdateUITheme is a decent work around
    if (event->type() == QEvent::PaletteChange ||
        event->type() == QEvent::ApplicationPaletteChange) {
        LOG_DEBUG(Frontend,
                  "Window color palette changed by event: {} (QEvent::PaletteChange is: {})",
                  event->type(), QEvent::PaletteChange);
        const QPalette test_palette(qApp->palette());
        // Keeping eye on QPalette::Window to avoid looping. QPalette::Text might be useful too
        const QColor window_color = test_palette.color(QPalette::Active, QPalette::Window);

        if (last_window_color != window_color) {
            last_window_color = window_color;

            UpdateUITheme();
        }
    }
    QWidget::changeEvent(event);
}

void GMainWindow::LoadTranslation() {
    bool loaded;

    if (UISettings::values.language.GetValue().empty()) {
        // If the selected language is empty, use system locale
        loaded = translator.load(QLocale(), {}, {}, QStringLiteral(":/languages/"));
    } else {
        // Otherwise load from the specified file
        loaded = translator.load(QString::fromStdString(UISettings::values.language.GetValue()),
                                 QStringLiteral(":/languages/"));
    }

    if (loaded) {
        qApp->installTranslator(&translator);
    } else {
        UISettings::values.language = std::string("en");
    }
}

void GMainWindow::OnLanguageChanged(const QString& locale) {
    if (UISettings::values.language.GetValue() != std::string("en")) {
        qApp->removeTranslator(&translator);
    }

    UISettings::values.language = locale.toStdString();
    LoadTranslation();
    ui->retranslateUi(this);
    multiplayer_state->retranslateUi();
    UpdateWindowTitle();
}

void GMainWindow::SetDiscordEnabled([[maybe_unused]] bool state) {
#ifdef USE_DISCORD_PRESENCE
    if (state) {
        discord_rpc = std::make_unique<DiscordRPC::DiscordImpl>(*system);
    } else {
        discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
    }
#else
    discord_rpc = std::make_unique<DiscordRPC::NullImpl>();
#endif
    discord_rpc->Update();
}

#ifdef __unix__
void GMainWindow::SetGamemodeEnabled(bool state) {
    if (emulation_running) {
        Common::Linux::SetGamemodeState(state);
    }
}
#endif

Service::AM::FrontendAppletParameters GMainWindow::ApplicationAppletParameters() {
    return Service::AM::FrontendAppletParameters{
        .applet_id = Service::AM::AppletId::Application,
        .applet_type = Service::AM::AppletType::Application,
    };
}

Service::AM::FrontendAppletParameters GMainWindow::LibraryAppletParameters(
    u64 program_id, Service::AM::AppletId applet_id) {
    return Service::AM::FrontendAppletParameters{
        .program_id = program_id,
        .applet_id = applet_id,
        .applet_type = Service::AM::AppletType::LibraryApplet,
    };
}

void VolumeButton::wheelEvent(QWheelEvent* event) {

    int num_degrees = event->angleDelta().y() / 8;
    int num_steps = (num_degrees / 15) * scroll_multiplier;
    // Stated in QT docs: Most mouse types work in steps of 15 degrees, in which case the delta
    // value is a multiple of 120; i.e., 120 units * 1/8 = 15 degrees.

    if (num_steps > 0) {
        Settings::values.volume.SetValue(
            std::min(200, Settings::values.volume.GetValue() + num_steps));
    } else {
        Settings::values.volume.SetValue(
            std::max(0, Settings::values.volume.GetValue() + num_steps));
    }

    scroll_multiplier = std::min(MaxMultiplier, scroll_multiplier * 2);
    scroll_timer.start(100); // reset the multiplier if no scroll event occurs within 100 ms

    emit VolumeChanged();
    event->accept();
}

void VolumeButton::ResetMultiplier() {
    scroll_multiplier = 1;
}

static void SetHighDPIAttributes() {
#ifdef _WIN32
    // For Windows, we want to avoid scaling artifacts on fractional scaling ratios.
    // This is done by setting the optimal scaling policy for the primary screen.

    // Create a temporary QApplication.
    int temp_argc = 0;
    char** temp_argv = nullptr;
    QApplication temp{temp_argc, temp_argv};

    // Get the current screen geometry.
    const QScreen* primary_screen = QGuiApplication::primaryScreen();
    if (primary_screen == nullptr) {
        return;
    }

    const QRect screen_rect = primary_screen->geometry();
    const int real_width = screen_rect.width();
    const int real_height = screen_rect.height();
    const float real_ratio = primary_screen->logicalDotsPerInch() / 96.0f;

    // Recommended minimum width and height for proper window fit.
    constexpr float minimum_width = 1350.0f;
    constexpr float minimum_height = 900.0f;

    const float width_ratio = std::max(1.0f, real_width / minimum_width);
    const float height_ratio = std::max(1.0f, real_height / minimum_height);

    // Get the lower of the 2 ratios and truncate, this is the maximum integer scale.
    const float max_ratio = std::trunc(std::min(width_ratio, height_ratio));

    if (max_ratio > real_ratio) {
        QApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::Round);
    } else {
        QApplication::setHighDpiScaleFactorRoundingPolicy(
            Qt::HighDpiScaleFactorRoundingPolicy::Floor);
    }
#else
    // Other OSes should be better than Windows at fractional scaling.
    QApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
#endif

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
}

int main(int argc, char* argv[]) {
    std::unique_ptr<QtConfig> config = std::make_unique<QtConfig>();
    bool has_broken_vulkan = false;
    bool is_child = false;
    InstallTerminateLogger();
#ifdef _WIN32
    SetUnhandledExceptionFilter(LogUnhandledSehException);
#endif
    if (CheckEnvVars(&is_child)) {
        return 0;
    }

    if (StartupChecks(argv[0], &has_broken_vulkan,
                      Settings::values.perform_vulkan_check.GetValue())) {
        return 0;
    }

#ifdef SUYU_CRASH_DUMPS
    Breakpad::InstallCrashHandler();
#endif

    Common::DetachedTasks detached_tasks;
    MicroProfileOnThreadCreate("Frontend");
    SCOPE_EXIT {
        MicroProfileShutdown();
    };

    Common::ConfigureNvidiaEnvironmentFlags();

    // Init settings params
    QCoreApplication::setOrganizationName(QStringLiteral("suyu team"));
    QCoreApplication::setApplicationName(QStringLiteral("suyu"));

#ifdef _WIN32
    QByteArray current_qt_qpa = qgetenv("QT_QPA_PLATFORM");
    // Follow dark mode setting, if the "-platform" launch option is not set.
    // Otherwise, just follow dark mode for the window decoration (title bar).
    if (!current_qt_qpa.contains(":darkmode=")) {
        if (UISettings::values.dark_mode_state == DarkModeState::Auto) {
            // When setting is Auto, force adapting window decoration and stylesheet palette to use
            // Windows theme. Default is darkmode:0, which always uses light palette
            if (current_qt_qpa.isEmpty()) {
                // Set the value
                qputenv("QT_QPA_PLATFORM", QByteArray("windows:darkmode=2"));
            } else {
                // Concatenate to the existing value
                qputenv("QT_QPA_PLATFORM", QByteArray(current_qt_qpa + ",darkmode=2"));
            }
        } else {
            // When setting is no Auto, adapt window decoration to the palette used
            if (current_qt_qpa.isEmpty()) {
                // Set the value
                qputenv("QT_QPA_PLATFORM", QByteArray("windows:darkmode=1"));
            } else {
                // Concatenate to the existing value
                qputenv("QT_QPA_PLATFORM", QByteArray(current_qt_qpa + ",darkmode=1"));
            }
        }
    }
    // Increases the maximum open file limit to 8192
    _setmaxstdio(8192);
#endif

#ifdef __APPLE__
    // If you start a bundle (binary) on OSX without the Terminal, the working directory is "/".
    // But since we require the working directory to be the executable path for the location of
    // the user folder in the Qt Frontend, we need to cd into that working directory
    const auto bin_path = Common::FS::GetBundleDirectory() / "..";
    chdir(Common::FS::PathToUTF8String(bin_path).c_str());
#endif

#ifdef __linux__
    // Set the DISPLAY variable in order to open web browsers
    // TODO (lat9nq): Find a better solution for AppImages to start external applications
    if (QString::fromLocal8Bit(qgetenv("DISPLAY")).isEmpty()) {
        qputenv("DISPLAY", ":0");
    }

    // Fix the Wayland appId. This needs to match the name of the .desktop file without the .desktop
    // suffix.
    QGuiApplication::setDesktopFileName(QStringLiteral("dev.suyu_emu.suyu"));
#endif

    SetHighDPIAttributes();

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Disables the "?" button on all dialogs. Disabled by default on Qt6.
    QCoreApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

    // Enables the core to make the qt created contexts current on std::threads
    QCoreApplication::setAttribute(Qt::AA_DontCheckOpenGLContextThreadAffinity);

    QApplication app(argc, argv);

    UISettings::RestoreWindowState(config);

#ifdef _WIN32
    OverrideWindowsFont();
#endif

    // Workaround for QTBUG-85409, for Suzhou numerals the number 1 is actually \u3021
    // so we can see if we get \u3008 instead
    // TL;DR all other number formats are consecutive in unicode code points
    // This bug is fixed in Qt6, specifically 6.0.0-alpha1
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    const QLocale locale = QLocale::system();
    if (QStringLiteral("\u3008") == locale.toString(1)) {
        QLocale::setDefault(QLocale::system().name());
    }
#endif

    // Qt changes the locale and causes issues in float conversion using std::to_string() when
    // generating shaders
    setlocale(LC_ALL, "C");

    GMainWindow main_window{std::move(config), has_broken_vulkan};
    // After settings have been loaded by GMainWindow, apply the filter
    main_window.show();

    const QStringList launch_args = QCoreApplication::arguments();
    std::optional<AppMode> requested_mode;
    for (int i = 1; i < launch_args.size(); ++i) {
        if (launch_args[i] == QStringLiteral("-gamer")) {
            requested_mode = AppMode::Gamer;
        } else if (launch_args[i] == QStringLiteral("-hacker")) {
            requested_mode = AppMode::Hacker;
        } else if (launch_args[i] == QStringLiteral("-programmer")) {
            requested_mode = AppMode::Programmer;
        }
    }

    // Show mode selector on first launch or when not remembered, unless a mode was requested
    // explicitly for unattended startup or MCP-driven automation.
    {
        AppMode active_mode = requested_mode.value_or(ModeSelector::LoadSavedMode());
        QSettings settings;
        const bool remember = settings.value(QStringLiteral("General/RememberMode"), false).toBool();
        if (!requested_mode.has_value() && !remember) {
            ModeSelector selector;
            if (selector.exec() == QDialog::Accepted) {
                active_mode = selector.SelectedMode();
            }
        }
        main_window.ApplyAppMode(active_mode);
    }

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                     &GMainWindow::OnAppFocusStateChanged);

    int result = app.exec();
    detached_tasks.WaitForAllTasks();
    return result;
}
