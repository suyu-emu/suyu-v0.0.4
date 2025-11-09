// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2014 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>

#include <filesystem>
#include <QMainWindow>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QTranslator>

#include "common/common_types.h"
#include "frontend_common/content_manager.h"
#include "input_common/drivers/tas_input.h"
#include "qt_common/config/qt_config.h"
#include "qt_common/util/game.h"
#include "yuzu/user_data_migration.h"
#include "yuzu/compatibility_list.h"
#include "yuzu/hotkeys.h"

#ifdef __unix__
#include <QDBusObjectPath>
#include <QVariant>
#include <QtDBus/QDBusInterface>
#include <QSocketNotifier>
#endif

#ifdef ENABLE_UPDATE_CHECKER
#include <QFuture>
#include <QFutureWatcher>
#endif

class QtConfig;
class EmuThread;
class GameList;
class GImageInfo;
class GRenderWindow;
class LoadingScreen;
class OverlayDialog;
class ControllerDialog;
class QLabel;
class MultiplayerState;
class QPushButton;
class QProgressDialog;
class QSlider;
class QHBoxLayout;
class WaitTreeWidget;
enum class GameListOpenTarget;
enum class DumpRomFSTarget;
class GameListPlaceholder;

class QtAmiiboSettingsDialog;
class QtControllerSelectorDialog;
class QtProfileSelectionDialog;
class QtSoftwareKeyboardDialog;
class QtNXWebEngineView;

enum class StartGameType {
    Normal, // Can use custom configuration
    Global, // Only uses global configuration
};

namespace Core {
enum class SystemResultStatus : u32;
} // namespace Core

namespace Core::Frontend {
struct CabinetParameters;
struct ControllerParameters;
struct InlineAppearParameters;
struct InlineTextParameters;
struct KeyboardInitializeParameters;
struct ProfileSelectParameters;
} // namespace Core::Frontend

namespace DiscordRPC {
class DiscordInterface;
}

namespace PlayTime {
class PlayTimeManager;
}

namespace FileSys {
class ContentProvider;
class ManualContentProvider;
class VfsFilesystem;
} // namespace FileSys

namespace InputCommon {
class InputSubsystem;
}

namespace Service::AM {
struct FrontendAppletParameters;
enum class AppletId : u32;
} // namespace Service::AM

namespace Service::AM::Frontend {
enum class SwkbdResult : u32;
enum class SwkbdTextCheckResult : u32;
enum class SwkbdReplyType : u32;
enum class WebExitReason : u32;
} // namespace Service::AM::Frontend

namespace Service::NFC {
class NfcDevice;
} // namespace Service::NFC

namespace Service::NFP {
enum class CabinetMode : u8;
} // namespace Service::NFP

namespace Ui {
class MainWindow;
}

enum class EmulatedDirectoryTarget {
    NAND,
    SDMC,
};

namespace VkDeviceInfo {
class Record;
}

class VolumeButton : public QPushButton {
    Q_OBJECT
public:
    explicit VolumeButton(QWidget* parent = nullptr) : QPushButton(parent), scroll_multiplier(1) {
        connect(&scroll_timer, &QTimer::timeout, this, &VolumeButton::ResetMultiplier);
    }

signals:
    void VolumeChanged();

protected:
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void ResetMultiplier();

private:
    int scroll_multiplier;
    QTimer scroll_timer;
    constexpr static int MaxMultiplier = 8;
};

class MainWindow : public QMainWindow {
    Q_OBJECT

    /// Max number of recently loaded items to keep track of
    static const int max_recent_files_item = 10;

public:
    void filterBarSetChecked(bool state);
    void UpdateUITheme();
    explicit MainWindow(bool has_broken_vulkan);
    ~MainWindow() override;

    bool DropAction(QDropEvent* event);
    void AcceptDropEvent(QDropEvent* event);

    std::filesystem::path GetShortcutPath(QtCommon::Game::ShortcutTarget target);

signals:

    /**
     * Signal that is emitted when a new EmuThread has been created and an emulation session is
     * about to start. At this time, the core system emulation has been initialized, and all
     * emulation handles and memory should be valid.
     *
     * @param emu_thread Pointer to the newly created EmuThread (to be used by widgets that need to
     *      access/change emulation state).
     */
    void EmulationStarting(EmuThread* emu_thread);

    /**
     * Signal that is emitted when emulation is about to stop. At this time, the EmuThread and core
     * system emulation handles and memory are still valid, but are about become invalid.
     */
    void EmulationStopping();

    // Signal that tells widgets to update icons to use the current theme
    void UpdateThemedIcons();

    void UpdateInstallProgress();

    void AmiiboSettingsFinished(bool is_success, const std::string& name);

    void ControllerSelectorReconfigureFinished(bool is_success);

    void ErrorDisplayFinished();

    void ProfileSelectorFinishedSelection(std::optional<Common::UUID> uuid);

    void SoftwareKeyboardSubmitNormalText(Service::AM::Frontend::SwkbdResult result,
                                          std::u16string submitted_text, bool confirmed);
    void SoftwareKeyboardSubmitInlineText(Service::AM::Frontend::SwkbdReplyType reply_type,
                                          std::u16string submitted_text, s32 cursor_position);

    void WebBrowserExtractOfflineRomFS();
    void WebBrowserClosed(Service::AM::Frontend::WebExitReason exit_reason, std::string last_url);

    void SigInterrupt();

public slots:
    void OnLoadComplete();
    void OnExecuteProgram(std::size_t program_index);
    void OnExit();
    void OnSaveConfig();
    void AmiiboSettingsShowDialog(const Core::Frontend::CabinetParameters& parameters,
                                  std::shared_ptr<Service::NFC::NfcDevice> nfp_device);
    void AmiiboSettingsRequestExit();
    void ControllerSelectorReconfigureControllers(
        const Core::Frontend::ControllerParameters& parameters);
    void ControllerSelectorRequestExit();
    void SoftwareKeyboardInitialize(
        bool is_inline, Core::Frontend::KeyboardInitializeParameters initialize_parameters);
    void SoftwareKeyboardShowNormal();
    void SoftwareKeyboardShowTextCheck(
        Service::AM::Frontend::SwkbdTextCheckResult text_check_result,
        std::u16string text_check_message);
    void SoftwareKeyboardShowInline(Core::Frontend::InlineAppearParameters appear_parameters);
    void SoftwareKeyboardHideInline();
    void SoftwareKeyboardInlineTextChanged(Core::Frontend::InlineTextParameters text_parameters);
    void SoftwareKeyboardExit();
    void ErrorDisplayDisplayError(QString error_code, QString error_text);
    void ErrorDisplayRequestExit();
    void ProfileSelectorSelectProfile(const Core::Frontend::ProfileSelectParameters& parameters);
    void ProfileSelectorRequestExit();
    void WebBrowserOpenWebPage(const std::string& main_url, const std::string& additional_args,
                               bool is_local);
    void WebBrowserRequestExit();
    void OnAppFocusStateChanged(Qt::ApplicationState state);
    void OnTasStateChanged();

private:
    /// Updates an action's shortcut and text to reflect an updated hotkey from the hotkey registry.
    void LinkActionShortcut(QAction* action, const QString& action_name,
                            const bool tas_allowed = false);

    void InitializeWidgets();
    void InitializeDebugWidgets();
    void InitializeRecentFileMenuActions();

    void SetDefaultUIGeometry();
    void RestoreUIState();

    void ConnectWidgetEvents();
    void ConnectMenuEvents();
    void UpdateMenuState();

    void SetupPrepareForSleep();

    void PreventOSSleep();
    void AllowOSSleep();

    bool LoadROM(const QString& filename, Service::AM::FrontendAppletParameters params);
    void BootGame(const QString& filename, Service::AM::FrontendAppletParameters params,
                  StartGameType with_config = StartGameType::Normal);
    void BootGameFromList(const QString& filename, StartGameType with_config);
    void ShutdownGame();

    void SetDiscordEnabled(bool state);
    void LoadAmiibo(const QString& filename);

    bool SelectAndSetCurrentUser(const Core::Frontend::ProfileSelectParameters& parameters);

    /**
     * Stores the filename in the recently loaded files list.
     * The new filename is stored at the beginning of the recently loaded files list.
     * After inserting the new entry, duplicates are removed meaning that if
     * this was inserted from \a OnMenuRecentFile(), the entry will be put on top
     * and remove from its previous position.
     *
     * Finally, this function calls \a UpdateRecentFiles() to update the UI.
     *
     * @param filename the filename to store
     */
    void StoreRecentFile(const QString& filename);

    /**
     * Updates the recent files menu.
     * Menu entries are rebuilt from the configuration file.
     * If there is no entry in the menu, the menu is greyed out.
     */
    void UpdateRecentFiles();

    /**
     * If the emulation is running,
     * asks the user if he really want to close the emulator
     *
     * @return true if the user confirmed
     */
    bool ConfirmClose();
    bool ConfirmChangeGame();
    bool ConfirmForceLockedExit();
    void RequestGameExit();
    void changeEvent(QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

    std::string CreateTASFramesString(
        std::array<size_t, InputCommon::TasInput::PLAYER_NUMBER> frames) const;

#ifdef __unix__
    void SetupSigInterrupts();
    static void HandleSigInterrupt(int);
    void OnSigInterruptNotifierActivated();
    void SetGamemodeEnabled(bool state);
#endif

    Service::AM::FrontendAppletParameters ApplicationAppletParameters();
    Service::AM::FrontendAppletParameters LibraryAppletParameters(u64 program_id,
                                                                  Service::AM::AppletId applet_id);

private slots:
    void OnStartGame();
    void OnRestartGame();
    void OnPauseGame();
    void OnPauseContinueGame();
    void OnStopGame();
    void OnPrepareForSleep(bool prepare_sleep);
    void OnMenuReportCompatibility();
    void OnOpenModsPage();
    void OnOpenQuickstartGuide();
    void OnOpenFAQ();

    /// Called whenever a user selects a game in the game list widget.
    void OnGameListLoadFile(QString game_path, u64 program_id);
    void OnGameListOpenFolder(u64 program_id, GameListOpenTarget target,
                              const std::string& game_path);
    void OnGameListRemoveInstalledEntry(u64 program_id, QtCommon::Game::InstalledEntryType type);
    void OnGameListRemoveFile(u64 program_id, QtCommon::Game::GameListRemoveTarget target,
                              const std::string& game_path);
    void OnGameListRemovePlayTimeData(u64 program_id);
    void OnGameListSetPlayTime(u64 program_id);
    void OnGameListDumpRomFS(u64 program_id, const std::string& game_path, DumpRomFSTarget target);
    void OnGameListVerifyIntegrity(const std::string& game_path);
    void OnGameListCopyTID(u64 program_id);
    void OnGameListNavigateToGamedbEntry(u64 program_id,
                                         const CompatibilityList& compatibility_list);
    void OnGameListCreateShortcut(u64 program_id,
                                  const std::string& game_path,
                                  const QtCommon::Game::ShortcutTarget target);
    void OnGameListOpenDirectory(const QString& directory);
    void OnGameListAddDirectory();
    void OnGameListShowList(bool show);
    void OnGameListOpenPerGameProperties(const std::string& file);
    void OnLinkToRyujinx(const u64& program_id);
    void OnMenuLoadFile();
    void OnMenuLoadFolder();
    void IncrementInstallProgress();
    void OnMenuInstallToNAND();
    void OnMenuRecentFile();
    void OnConfigure();
    void OnConfigureTas();
    void OnDecreaseVolume();
    void OnIncreaseVolume();
    void OnMute();
    void OnTasStartStop();
    void OnTasRecord();
    void OnTasReset();
    void OnToggleGraphicsAPI();
    void OnToggleDockedMode();
    void OnToggleGpuAccuracy();
    void OnToggleAdaptingFilter();
    void OnConfigurePerGame();
    void OnLoadAmiibo();
    void OnOpenRootDataFolder();
    void OnOpenNANDFolder();
    void OnOpenSDMCFolder();
    void OnOpenModFolder();
    void OnOpenLogFolder();
    void OnVerifyInstalledContents();
    void OnInstallFirmware();
    void OnInstallFirmwareFromZIP();
    void OnInstallDecryptionKeys();
    void OnAbout();
    void OnEdenDependencies();
    void OnDataDialog();
    void OnToggleFilterBar();
    void OnToggleStatusBar();
    void OnGameListRefresh();
    void InitializeHotkeys();
    void ToggleFullscreen();
    bool UsingExclusiveFullscreen();
    void ShowFullscreen();
    void HideFullscreen();
    void ToggleWindowMode();
    void ResetWindowSize(u32 width, u32 height);
    void ResetWindowSize720();
    void ResetWindowSize900();
    void ResetWindowSize1080();
    void OnAlbum();
    void OnCabinet(Service::NFP::CabinetMode mode);
    void OnMiiEdit();
    void OnOpenControllerMenu();
    void OnHomeMenu();
    void OnInitialSetup();
    void OnCreateHomeMenuDesktopShortcut();
    void OnCreateHomeMenuApplicationMenuShortcut();
    void OnCaptureScreenshot();
    void OnCheckFirmwareDecryption();
#ifdef __unix__
    void OnCheckGraphicsBackend();
#endif
    void OnLanguageChanged(const QString& locale);
    void OnMouseActivity();
    bool OnShutdownBegin();
    void OnShutdownBeginDialog();
    void OnEmulationStopped();
    void OnEmulationStopTimeExpired();

#ifdef ENABLE_UPDATE_CHECKER
    void OnEmulatorUpdateAvailable();
#endif

private:
    void RemovePlayTimeData(u64 program_id);
    bool SelectRomFSDumpTarget(const FileSys::ContentProvider&, u64 program_id,
                               u64* selected_title_id, u8* selected_content_record_type);
    ContentManager::InstallResult InstallNCA(const QString& filename);
    void MigrateConfigFiles();
    void UpdateWindowTitle(std::string_view title_name = {}, std::string_view title_version = {},
                           std::string_view gpu_vendor = {});
    void UpdateDockedButton();
    void UpdateAPIText();
    void UpdateFilterText();
    void UpdateAAText();
    void UpdateVolumeUI();
    void UpdateStatusBar();
    void UpdateGPUAccuracyButton();
    void UpdateStatusButtons();
    void UpdateUISettings();
    void UpdateInputDrivers();
    void HideMouseCursor();
    void ShowMouseCursor();
    void OpenURL(const QUrl& url);
    void LoadTranslation();
    void OpenPerGameConfiguration(u64 title_id, const std::string& file_name);
    bool CheckFirmwarePresence();
    void SetFirmwareVersion();
    void ConfigureFilesystemProvider(const std::string& filepath);
    /**
     * Open (or not) the right confirm dialog based on current setting and game exit lock
     * @returns true if the player confirmed or the settings do no require it
     */
    bool ConfirmShutdownGame();

    QString GetTasStateDescription() const;
    bool CreateShortcutMessagesGUI(QWidget* parent, int imsg, const QString& game_title);

    /**
     * Mimic the behavior of QMessageBox::question but link controller navigation to the dialog
     * The only difference is that it returns a boolean.
     *
     * @returns true if buttons contains QMessageBox::Yes and the user clicks on the "Yes" button.
     */
    bool question(QWidget* parent, const QString& title, const QString& text,
                  QMessageBox::StandardButtons buttons =
                  QMessageBox::StandardButtons(QMessageBox::Yes | QMessageBox::No),
                  QMessageBox::StandardButton defaultButton = QMessageBox::NoButton);

    std::unique_ptr<Ui::MainWindow> ui;

    std::unique_ptr<DiscordRPC::DiscordInterface> discord_rpc;
    std::unique_ptr<PlayTime::PlayTimeManager> play_time_manager;
    std::shared_ptr<InputCommon::InputSubsystem> input_subsystem;

#ifdef ENABLE_UPDATE_CHECKER
    QFuture<QString> update_future;
    QFutureWatcher<QString> update_watcher;
#endif

    MultiplayerState* multiplayer_state = nullptr;

    GRenderWindow* render_window;
    GameList* game_list;
    LoadingScreen* loading_screen;
    QTimer shutdown_timer;
    OverlayDialog* shutdown_dialog{};

    GameListPlaceholder* game_list_placeholder;

    std::vector<VkDeviceInfo::Record> vk_device_records;

    // Status bar elements
    QLabel* message_label = nullptr;
    QLabel* shader_building_label = nullptr;
    QLabel* res_scale_label = nullptr;
    QLabel* emu_speed_label = nullptr;
    QLabel* game_fps_label = nullptr;
    QLabel* emu_frametime_label = nullptr;
    QLabel* tas_label = nullptr;
    QLabel* firmware_label = nullptr;
    QPushButton* gpu_accuracy_button = nullptr;
    QPushButton* renderer_status_button = nullptr;
    QPushButton* refresh_button = nullptr;
    QPushButton* dock_status_button = nullptr;
    QPushButton* filter_status_button = nullptr;
    QPushButton* aa_status_button = nullptr;
    VolumeButton* volume_button = nullptr;
    QWidget* volume_popup = nullptr;
    QSlider* volume_slider = nullptr;
    QTimer status_bar_update_timer;

    UserDataMigrator user_data_migrator;
    std::unique_ptr<QtConfig> config;

    // Whether emulation is currently running in yuzu.
    bool emulation_running = false;
    std::unique_ptr<EmuThread> emu_thread;
    // The path to the game currently running
    QString current_game_path;
    // Whether a user was set on the command line (skips UserSelector if it's forced to show up)
    bool user_flag_cmd_line = false;

    bool auto_paused = false;
    bool auto_muted = false;
    QTimer mouse_hide_timer;
    QTimer update_input_timer;

    QString startup_icon_theme;

    // Debugger panes
    ControllerDialog* controller_dialog;

    QAction* actions_recent_files[max_recent_files_item];

    // stores default icon theme search paths for the platform
    QStringList default_theme_paths;

    HotkeyRegistry hotkey_registry;

    QTranslator translator;

    // Install progress dialog
    QProgressDialog* install_progress;

    // Last game booted, used for multi-process apps
    QString last_filename_booted;

    // Applets
    QtAmiiboSettingsDialog* cabinet_applet = nullptr;
    QtControllerSelectorDialog* controller_applet = nullptr;
    QtProfileSelectionDialog* profile_select_applet = nullptr;
    QDialog* error_applet = nullptr;
    QtSoftwareKeyboardDialog* software_keyboard = nullptr;
    QtNXWebEngineView* web_applet = nullptr;

    // True if amiibo file select is visible
    bool is_amiibo_file_select_active{};

    // True if load file select is visible
    bool is_load_file_select_active{};

    // True if TAS recording dialog is visible
    bool is_tas_recording_dialog_active{};

#ifdef __unix__
    QSocketNotifier* sig_interrupt_notifier;
    static std::array<int, 3> sig_interrupt_fds;
#endif

    std::filesystem::path GetEdenCommand();

    void CreateShortcut(const std::string& game_path,
                        const u64 program_id,
                        const std::string& game_title,
                        QtCommon::Game::ShortcutTarget target,
                        std::string arguments,
                        const bool needs_title);

    void InstallFirmware(const QString& location, bool recursive = false);

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
};
