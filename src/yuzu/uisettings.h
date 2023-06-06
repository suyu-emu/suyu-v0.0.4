// SPDX-FileCopyrightText: 2016 Citra Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <vector>
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include "common/common_types.h"
#include "common/settings.h"

using Settings::Setting;

namespace UISettings {

bool IsDarkTheme();

struct ContextualShortcut {
    QString keyseq;
    QString controller_keyseq;
    int context;
    bool repeat;
};

struct Shortcut {
    QString name;
    QString group;
    ContextualShortcut shortcut;
};

enum class Theme {
    Default,
    DefaultColorful,
    Dark,
    DarkColorful,
    MidnightBlue,
    MidnightBlueColorful,
};

using Themes = std::array<std::pair<const char*, const char*>, 6>;
extern const Themes themes;

struct GameDir {
    QString path;
    bool deep_scan = false;
    bool expanded = false;
    bool operator==(const GameDir& rhs) const {
        return path == rhs.path;
    }
    bool operator!=(const GameDir& rhs) const {
        return !operator==(rhs);
    }
};

struct Values {
    QByteArray geometry;
    QByteArray state;

    QByteArray renderwindow_geometry;

    QByteArray gamelist_header_state;

    QByteArray microprofile_geometry;
    Setting<bool> microprofile_visible{false, "microProfileDialogVisible"};

    Setting<bool> single_window_mode{true, "singleWindowMode"};
    Setting<bool> fullscreen{false, "fullscreen"};
    Setting<bool> display_titlebar{true, "displayTitleBars"};
    Setting<bool> show_filter_bar{true, "showFilterBar"};
    Setting<bool> show_status_bar{true, "showStatusBar"};

    Setting<bool> confirm_before_closing{true, "confirmClose"};
    Setting<bool> first_start{true, "firstStart"};
    Setting<bool> pause_when_in_background{false, "pauseWhenInBackground"};
    Setting<bool> mute_when_in_background{false, "muteWhenInBackground"};
    Setting<bool> hide_mouse{true, "hideInactiveMouse"};
    Setting<bool> controller_applet_disabled{false, "disableControllerApplet"};
    // Set when Vulkan is known to crash the application
    bool has_broken_vulkan = false;

    Setting<bool> select_user_on_boot{false, "select_user_on_boot"};
    Setting<bool> disable_web_applet{true, "disable_web_applet"};

    // Discord RPC
    Setting<bool> enable_discord_presence{true, "enable_discord_presence"};

    // logging
    Setting<bool> show_console{false, "showConsole"};

    Setting<bool> enable_screenshot_save_as{true, "enable_screenshot_save_as"};

    QString roms_path;
    QString symbols_path;
    QString game_dir_deprecated;
    bool game_dir_deprecated_deepscan;
    QVector<UISettings::GameDir> game_dirs;
    QStringList recent_files;
    QString language;

    QString theme;

    // Shortcut name <Shortcut, context>
    std::vector<Shortcut> shortcuts;

    Setting<u32> callout_flags{0, "calloutFlags"};

    // multiplayer settings
    Setting<std::string> multiplayer_nickname{{}, "nickname"};
    Setting<std::string> multiplayer_ip{{}, "ip"};
    Setting<u16, true> multiplayer_port{24872, 0, UINT16_MAX, "port"};
    Setting<std::string> multiplayer_room_nickname{{}, "room_nickname"};
    Setting<std::string> multiplayer_room_name{{}, "room_name"};
    Setting<u8, true> multiplayer_max_player{8, 0, 8, "max_player"};
    Setting<u16, true> multiplayer_room_port{24872, 0, UINT16_MAX, "room_port"};
    Setting<u8, true> multiplayer_host_type{0, 0, 1, "host_type"};
    Setting<unsigned long long> multiplayer_game_id{{}, "game_id"};
    Setting<std::string> multiplayer_room_description{{}, "room_description"};
    std::pair<std::vector<std::string>, std::vector<std::string>> multiplayer_ban_list;

    // Game List
    Setting<bool> show_add_ons{true, "show_add_ons"};
    Setting<u32> game_icon_size{64, "game_icon_size"};
    Setting<u32> folder_icon_size{48, "folder_icon_size"};
    Setting<u8> row_1_text_id{3, "row_1_text_id"};
    Setting<u8> row_2_text_id{2, "row_2_text_id"};
    std::atomic_bool is_game_list_reload_pending{false};
    Setting<bool> cache_game_list{true, "cache_game_list"};
    Setting<bool> favorites_expanded{true, "favorites_expanded"};
    QVector<u64> favorited_ids;

    // Compatibility List
    Setting<bool> show_compat{false, "show_compat"};

    // Size & File Types Column
    Setting<bool> show_size{true, "show_size"};
    Setting<bool> show_types{true, "show_types"};

    bool configuration_applied;
    bool reset_to_defaults;
    bool shortcut_already_warned{false};
};

extern Values values;

} // namespace UISettings

Q_DECLARE_METATYPE(UISettings::GameDir*);
