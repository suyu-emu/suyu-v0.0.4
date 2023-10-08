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
#include "common/settings_enums.h"

using Settings::Category;
using Settings::Setting;

#ifndef CANNOT_EXPLICITLY_INSTANTIATE
namespace Settings {
extern template class Setting<bool>;
extern template class Setting<std::string>;
extern template class Setting<u16, true>;
extern template class Setting<u32>;
extern template class Setting<u8, true>;
extern template class Setting<u8>;
extern template class Setting<unsigned long long>;
} // namespace Settings
#endif

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
    Settings::Linkage linkage{1000};

    QByteArray geometry;
    QByteArray state;

    QByteArray renderwindow_geometry;

    QByteArray gamelist_header_state;

    QByteArray microprofile_geometry;
    Setting<bool> microprofile_visible{linkage, false, "microProfileDialogVisible",
                                       Category::UiLayout};

    Setting<bool> single_window_mode{linkage, true, "singleWindowMode", Category::Ui};
    Setting<bool> fullscreen{linkage, false, "fullscreen", Category::Ui};
    Setting<bool> display_titlebar{linkage, true, "displayTitleBars", Category::Ui};
    Setting<bool> show_filter_bar{linkage, true, "showFilterBar", Category::Ui};
    Setting<bool> show_status_bar{linkage, true, "showStatusBar", Category::Ui};

    Setting<bool> confirm_before_closing{
        linkage, true, "confirmClose", Category::UiGeneral, Settings::Specialization::Default,
        true,    true};
    Setting<bool> first_start{linkage, true, "firstStart", Category::Ui};
    Setting<bool> pause_when_in_background{linkage,
                                           false,
                                           "pauseWhenInBackground",
                                           Category::UiGeneral,
                                           Settings::Specialization::Default,
                                           true,
                                           true};
    Setting<bool> mute_when_in_background{
        linkage, false, "muteWhenInBackground", Category::Audio, Settings::Specialization::Default,
        true,    true};
    Setting<bool> hide_mouse{
        linkage, true, "hideInactiveMouse", Category::UiGeneral, Settings::Specialization::Default,
        true,    true};
    Setting<bool> controller_applet_disabled{linkage, false, "disableControllerApplet",
                                             Category::UiGeneral};
    // Set when Vulkan is known to crash the application
    bool has_broken_vulkan = false;

    Setting<bool> select_user_on_boot{linkage,
                                      false,
                                      "select_user_on_boot",
                                      Category::UiGeneral,
                                      Settings::Specialization::Default,
                                      true,
                                      true};
    Setting<bool> disable_web_applet{linkage, true, "disable_web_applet", Category::Ui};

    // Discord RPC
    Setting<bool> enable_discord_presence{linkage, true, "enable_discord_presence", Category::Ui};

    // logging
    Setting<bool> show_console{linkage, false, "showConsole", Category::Ui};

    // Screenshots
    Setting<bool> enable_screenshot_save_as{linkage, true, "enable_screenshot_save_as",
                                            Category::Screenshots};
    Setting<u32> screenshot_height{linkage, 0, "screenshot_height", Category::Screenshots};

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

    Setting<u32> callout_flags{linkage, 0, "calloutFlags", Category::Ui};

    // multiplayer settings
    Setting<std::string> multiplayer_nickname{linkage, {}, "nickname", Category::Multiplayer};
    Setting<std::string> multiplayer_ip{linkage, {}, "ip", Category::Multiplayer};
    Setting<u16, true> multiplayer_port{linkage,    24872,  0,
                                        UINT16_MAX, "port", Category::Multiplayer};
    Setting<std::string> multiplayer_room_nickname{
        linkage, {}, "room_nickname", Category::Multiplayer};
    Setting<std::string> multiplayer_room_name{linkage, {}, "room_name", Category::Multiplayer};
    Setting<u8, true> multiplayer_max_player{linkage, 8, 0, 8, "max_player", Category::Multiplayer};
    Setting<u16, true> multiplayer_room_port{linkage,    24872,       0,
                                             UINT16_MAX, "room_port", Category::Multiplayer};
    Setting<u8, true> multiplayer_host_type{linkage, 0, 0, 1, "host_type", Category::Multiplayer};
    Setting<unsigned long long> multiplayer_game_id{linkage, {}, "game_id", Category::Multiplayer};
    Setting<std::string> multiplayer_room_description{
        linkage, {}, "room_description", Category::Multiplayer};
    std::pair<std::vector<std::string>, std::vector<std::string>> multiplayer_ban_list;

    // Game List
    Setting<bool> show_add_ons{linkage, true, "show_add_ons", Category::UiGameList};
    Setting<u32> game_icon_size{linkage, 64, "game_icon_size", Category::UiGameList};
    Setting<u32> folder_icon_size{linkage, 48, "folder_icon_size", Category::UiGameList};
    Setting<u8> row_1_text_id{linkage, 3, "row_1_text_id", Category::UiGameList};
    Setting<u8> row_2_text_id{linkage, 2, "row_2_text_id", Category::UiGameList};
    std::atomic_bool is_game_list_reload_pending{false};
    Setting<bool> cache_game_list{linkage, true, "cache_game_list", Category::UiGameList};
    Setting<bool> favorites_expanded{linkage, true, "favorites_expanded", Category::UiGameList};
    QVector<u64> favorited_ids;

    // Compatibility List
    Setting<bool> show_compat{linkage, false, "show_compat", Category::UiGameList};

    // Size & File Types Column
    Setting<bool> show_size{linkage, true, "show_size", Category::UiGameList};
    Setting<bool> show_types{linkage, true, "show_types", Category::UiGameList};

    // Play time
    Setting<bool> show_play_time{linkage, true, "show_play_time", Category::UiGameList};

    bool configuration_applied;
    bool reset_to_defaults;
    bool shortcut_already_warned{false};
};

extern Values values;

u32 CalculateWidth(u32 height, Settings::AspectRatio ratio);

} // namespace UISettings

Q_DECLARE_METATYPE(UISettings::GameDir*);
