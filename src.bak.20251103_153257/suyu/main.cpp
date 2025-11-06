// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cinttypes>
#include <clocale>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
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
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QString>
#include <QSysInfo>
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
#include <shlobj.h>
#include "common/windows/timer_resolution.h"
#endif
#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_detect.h"
#endif
#include "common/settings.h"
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
#include "suyu/util/clickable_label.h"
#include "suyu/vk_device_info.h"
#include "ui_main.h"
#include "util/overlay_dialog.h"
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
    const auto& caps = Common::GetCPUCaps();
    std::string cpu_string = caps.cpu_string;
    if (caps.avx || caps.avx2 || caps.avx512f) {
        cpu_string += " | AVX";
        if (caps.avx512f) {
            cpu_string += "512";
        } else if (caps.avx2) {
            cpu_string += '2';
        }
        if (caps.fma || caps.fma4) {
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

    game_list->LoadCompatibilityList();
    game_list->PopulateAsync(UISettings::values.game_dirs);

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
                                "href='https://suyu.dev/wiki/faq/"
                                "#suyu-starts-with-the-error-broken-vulkan-installation-detected'>"
                                "here for instructions to fix the issue</a>."));

#ifdef HAS_OPENGL
        Settings::values.renderer_backend = Settings::RendererBackend::OpenGL;
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

    QStringList args = QApplication::arguments();

    if (args.size() < 2) {
        return;
    }

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
    ui->horizontalLayout->addWidget(game_list);

    game_list_placeholder = new GameListPlaceholder(this);
    ui->horizontalLayout->addWidget(game_list_placeholder);
    game_list_placeholder->setVisible(false);

    loading_screen = new LoadingScreen(this);
    loading_screen->hide();
    ui->horizontalLayout->addWidget(loading_screen);
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
        if (aa_mode == Settings::AntiAliasing::MaxEnum) {
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
                    if (gpu_accuracy_pair.first == Settings::GpuAccuracy::Extreme) {
                        continue;
                    }
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

    // Emulation
    connect_menu(ui->action_Pause, &GMainWindow::OnPauseContinueGame);
    connect_menu(ui->action_Stop, &GMainWindow::OnStopGame);
    connect_menu(ui->action_Report_Compatibility, &GMainWindow::OnMenuReportCompatibility);
    connect_menu(ui->action_Open_Mods_Page, &GMainWindow::OnOpenModsPage);
    connect_menu(ui->action_Open_Quickstart_Guide, &GMainWindow::OnOpenQuickstartGuide);
    connect_menu(ui->action_Open_FAQ, &GMainWindow::OnOpenFAQ);
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

    connect_menu(ui->action_Reset_Window_Size_720, &GMainWindow::ResetWindowSize720);
    connect_menu(ui->action_Reset_Window_Size_800, &GMainWindow::ResetWindowSize800);
    connect_menu(ui->action_Reset_Window_Size_900, &GMainWindow::ResetWindowSize900);
    connect_menu(ui->action_Reset_Window_Size_1080, &GMainWindow::ResetWindowSize1080);
    ui->menu_Reset_Window_Size->addActions(
        {ui->action_Reset_Window_Size_720, ui->action_Reset_Window_Size_800,
         ui->action_ResealFile(qt_shader_cache_path));
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

    QDesktopServices::openUrl(QUrl(QStringLiteral("https://suyu.dev/game/") + directory));
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

    UISettings::GameDir game_dir{dir_path.toStdString(), false, true};
    if (!UISettings::values.game_dirs.contains(game_dir)) {
        UISettings::values.game_dirs.append(game_dir);
        game_list->PopulateAsync(UISettings::values.game_dirs);
    } else {
        LOG_WARNING(Frontend, "Selected directory is already in the game list");
    }

    OnSaveConfig();
}

void GMainWindow::OnGameListShowList(boowered_on) {
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

    // Check for installed keys, error out, suggest restart?
    if (!ContentManager::AreKeysPresent()) {
        QMessageBox::information(
            this, tr("Keys not installed"),
            tr("Install decryption keys and restart suyu before attempting to install firmware."));
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
    const Common::FS::DirEntryCallable callback =
        [&out](const std::filesystem::directory_entry& entry) {
            if (entry.path().has_extension() && entry.path().extension() == ".nca") {
                out.emplace_back(entry.path());
            }

            return true;
        };

    QtProgressCallback(100, 10);

    Common::FS::IterateDirEntries(firmware_source_path, callback, Common::FS::DirEntryFilter::File);
    if (out.size() <= 0) {
        progress.close();
        QMessageBox::warning(this, tr("Firmware install failed"),
                             tr("Unable to locate potential firmware NCA files"));
        return;
    }

    // Locate and erase the content of nand/system/Content/registered/*.nca, if any.
    auto sysnand_content_vdir = system->GetFileSystemController().GetSystemNANDContentDirectory();
    if (!sysnand_content_vdir->CleanSubdirectoryRecursive("registered")) {
        progress.close();
        QMessageBox::critical(this, tr("Firmware install failed"),
                              tr("Failed to delete one or more firmware file."));
        return;
    }

    LOG_INFO(Frontend,
             "Cleaned nand/system/Content/registered folder in preparation for new firmware.");

    QtProgressCallback(100, 20);

    auto firmware_vdir = sysnand_content_vdir->GetDirectoryRelative("registered");

    bool success = true;
    int i = 0;
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

    if (!success) {
        progress.close();
        QMessageBox::critical(this, tr("Firmware install failed"),
                              tr("One or more firmware files failed to copy into NAND."));
        return;
    }

    // Re-scan VFS for the newly placed firmware files.
    system->GetFileSystemController().CreateFactories(*vfs);

    auto VerifyFirmwareCallback = [&](size_t total_size, size_t processed_size) {
        progress.setValue(90 + static_cast<int>((processed_size * 10) / total_size));
        return progress.wasCanceled();
    };

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

    progress.close();
    OnCheckFirmwareDecryption();
}

void GMainWindow::OnInstallDecryptionKeys() {
    // Don't do this while emulation is running.
    if (emu_thread != nullptr && emu_thread->IsRunning()) {
        return;
    }

    const QString key_source_location = QFileDialog::getOpenFileName(
        this, tr("Select Dumped Keys Location"), {}, QStringLiteral("prod.keys (prod.keys)"), {},
        QFileDialog::ReadOnly);
    if (key_source_location.isEmpty()) {
        return;
    }

    // Verify that it contains prod.keys, title.keys and optionally, key_retail.bin
    LOG_INFO(Frontend, "Installing key files from {}", key_source_location.toStdString());

    const std::filesystem::path prod_key_path = key_source_location.toStdString();
    const std::filesystem::path key_source_path = prod_key_path.parent_path();
    if (!Common::FS::IsDir(key_source_path)) {
        return;
    }

    bool prod_keys_found = false;
    std::vector<std::filesystem::path> source_key_files;

    if (Common::FS::Exists(prod_key_path)) {
        prod_keys_found = true;
        source_key_files.emplace_back(prod_key_path);
    }

    if (Common::FS::Exists(key_source_path / "title.keys")) {
        source_key_files.emplace_back(key_source_path / "title.keys");
    }

    if (Common::FS::Exists(key_source_path / "key_retail.bin")) {
        source_key_files.emplace_back(key_source_path / "key_retail.bin");
    }

    // There should be at least prod.keys.
    if (source_key_files.empty() || !prod_keys_found) {
        QMessageBox::warning(this, tr("Decryption Keys install failed"),
                             tr("prod.keys is a required decryption key file."));
        return;
    }

    const auto suyu_keys_dir = Common::FS::GetSuyuPath(Common::FS::SuyuPath::KeysDir);
    for (auto key_file : source_key_files) {
        std::filesystem::path destination_key_file = suyu_keys_dir / key_file.filename();
        if (!std::filesystem::copy_file(key_file, destination_key_file,
                                        std::filesystem::copy_options::overwrite_existing)) {
            LOG_ERROR(Frontend, "Failed to copy file {} to {}", key_file.string(),
                      destination_key_file.string());
            QMessageBox::critical(this, tr("Decryption Keys install failed"),
                                  tr("One or more keys failed to copy."));
            return;
        }
    }

    // Reinitialize the key manager, re-read the vfs (for update/dlc files),
    // and re-populate the game list in the UI if the user has already added
    // game folders.
    Core::Crypto::KeyManager::Instance().ReloadKeys();
    system->GetFileSystemController().CreateFactories(*vfs);
    game_list->PopulateAsync(UISettings::values.game_dirs);

    if (ContentManager::AreKeysPresent()) {
        QMessageBox::information(this, tr("Decryption Keys install succeeded"),
                                 tr("Decryption Keys were successfully installed"));
    } else {
        QMessageBox::critical(
            this, tr("Decryption Keys install failed"),
            tr("Decryption Keys failed to initialize. Check that your dumping tools are "
               "up to date and re-dump keys."));
    }

    OnCheckFirmwareDecryption();
}

void GMainWindow::OnAbout() {
    AboutDialog aboutDialog(this);
    aboutDialog.exec();
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
    const auto branch_name = std::string(Common::g_scm_branch);
    const auto description = std::string(Common::g_scm_desc);
    const auto build_id = std::string(Common::g_build_id);

    const auto suyu_title = fmt::format("suyu | {}-{}", branch_name, description);
    const auto override_title =
        fmt::format(fmt::runtime(std::string(Common::g_title_bar_format_idle)), build_id);
    const auto window_title = override_title.empty() ? suyu_title : override_title;

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
    gpu_accuracy_button->setChecked(gpu_accuracy != Settings::GpuAccuracy::Normal);
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
    renderer_status_button->setText(
        api == Settings::RendererBackend::OpenGL
            ? tr("%1 %2").arg(renderer_status_text.toUpper(),
                              ConfigurationShared::shader_backend_texts_map
                                  .find(Settings::values.shader_backend.GetValue())
                                  ->second)
            : renderer_status_text.toUpper());
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
    if (!Settings:tenate to the existing value
                qputenv("QT_QPA_PLATFORM", current_qt_qpa + ",darkmode=2");
            }
        } else {
            // When setting is no Auto, adapt window decoration to the palette used
            if (current_qt_qpa.isEmpty()) {
                // Set the value
                qputenv("QT_QPA_PLATFORM", QByteArray("windows:darkmode=1"));
            } else {
                // Concatenate to the existing value
                qputenv("QT_QPA_PLATFORM", current_qt_qpa + ",darkmode=1");
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

    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &main_window,
                     &GMainWindow::OnAppFocusStateChanged);

    int result = app.exec();
    detached_tasks.WaitForAllTasks();
    return result;
}
