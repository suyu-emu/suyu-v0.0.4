// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings

object EmulationMenuSettings {
    private val preferences =
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    // These must match what is defined in src/core/settings.h
    const val LayoutOption_Default = 0
    const val LayoutOption_SingleScreen = 1
    const val LayoutOption_LargeScreen = 2
    const val LayoutOption_SideScreen = 3
    const val LayoutOption_MobilePortrait = 4
    const val LayoutOption_MobileLandscape = 5

    var joystickRelCenter: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER, true)
        set(value) {
            preferences.edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER, value)
                .apply()
        }
    var dpadSlide: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_DPAD_SLIDE, true)
        set(value) {
            preferences.edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_DPAD_SLIDE, value)
                .apply()
        }
    var hapticFeedback: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_HAPTICS, false)
        set(value) {
            preferences.edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_HAPTICS, value)
                .apply()
        }

    var landscapeScreenLayout: Int
        get() = preferences.getInt(
            Settings.PREF_MENU_SETTINGS_LANDSCAPE,
            LayoutOption_MobileLandscape
        )
        set(value) {
            preferences.edit()
                .putInt(Settings.PREF_MENU_SETTINGS_LANDSCAPE, value)
                .apply()
        }
    var showFps: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_SHOW_FPS, false)
        set(value) {
            preferences.edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_SHOW_FPS, value)
                .apply()
        }
    var showOverlay: Boolean
        get() = preferences.getBoolean(Settings.PREF_MENU_SETTINGS_SHOW_OVERLAY, true)
        set(value) {
            preferences.edit()
                .putBoolean(Settings.PREF_MENU_SETTINGS_SHOW_OVERLAY, value)
                .apply()
        }
}
