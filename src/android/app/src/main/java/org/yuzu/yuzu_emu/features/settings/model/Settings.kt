// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import android.text.TextUtils
import android.widget.Toast
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile

object Settings {
    private val context get() = YuzuApplication.appContext

    fun saveSettings(gameId: String = "") {
        if (TextUtils.isEmpty(gameId)) {
            Toast.makeText(
                context,
                context.getString(R.string.ini_saved),
                Toast.LENGTH_SHORT
            ).show()
            SettingsFile.saveFile(SettingsFile.FILE_NAME_CONFIG)
        } else {
            // TODO: Save custom game settings
            Toast.makeText(
                context,
                context.getString(R.string.gameid_saved, gameId),
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    enum class Category {
        Android,
        Audio,
        Core,
        Cpu,
        CpuDebug,
        CpuUnsafe,
        Renderer,
        RendererAdvanced,
        RendererDebug,
        System,
        SystemAudio,
        DataStorage,
        Debugging,
        DebuggingGraphics,
        Miscellaneous,
        Network,
        WebService,
        AddOns,
        Controls,
        Ui,
        UiGeneral,
        UiLayout,
        UiGameList,
        Screenshots,
        Shortcuts,
        Multiplayer,
        Services,
        Paths,
        MaxEnum
    }

    val settingsList = listOf<AbstractSetting>(
        *BooleanSetting.values(),
        *ByteSetting.values(),
        *ShortSetting.values(),
        *IntSetting.values(),
        *FloatSetting.values(),
        *LongSetting.values(),
        *StringSetting.values()
    )

    const val SECTION_GENERAL = "General"
    const val SECTION_SYSTEM = "System"
    const val SECTION_RENDERER = "Renderer"
    const val SECTION_AUDIO = "Audio"
    const val SECTION_CPU = "Cpu"
    const val SECTION_THEME = "Theme"
    const val SECTION_DEBUG = "Debug"

    enum class MenuTag(val titleId: Int) {
        SECTION_ROOT(R.string.advanced_settings),
        SECTION_SYSTEM(R.string.preferences_system),
        SECTION_RENDERER(R.string.preferences_graphics),
        SECTION_AUDIO(R.string.preferences_audio),
        SECTION_CPU(R.string.cpu),
        SECTION_THEME(R.string.preferences_theme),
        SECTION_DEBUG(R.string.preferences_debug);
    }

    const val PREF_MEMORY_WARNING_SHOWN = "MemoryWarningShown"

    const val PREF_OVERLAY_VERSION = "OverlayVersion"
    const val PREF_LANDSCAPE_OVERLAY_VERSION = "LandscapeOverlayVersion"
    const val PREF_PORTRAIT_OVERLAY_VERSION = "PortraitOverlayVersion"
    const val PREF_FOLDABLE_OVERLAY_VERSION = "FoldableOverlayVersion"
    val overlayLayoutPrefs = listOf(
        PREF_LANDSCAPE_OVERLAY_VERSION,
        PREF_PORTRAIT_OVERLAY_VERSION,
        PREF_FOLDABLE_OVERLAY_VERSION
    )

    const val PREF_CONTROL_SCALE = "controlScale"
    const val PREF_CONTROL_OPACITY = "controlOpacity"
    const val PREF_TOUCH_ENABLED = "isTouchEnabled"
    const val PREF_BUTTON_A = "buttonToggle0"
    const val PREF_BUTTON_B = "buttonToggle1"
    const val PREF_BUTTON_X = "buttonToggle2"
    const val PREF_BUTTON_Y = "buttonToggle3"
    const val PREF_BUTTON_L = "buttonToggle4"
    const val PREF_BUTTON_R = "buttonToggle5"
    const val PREF_BUTTON_ZL = "buttonToggle6"
    const val PREF_BUTTON_ZR = "buttonToggle7"
    const val PREF_BUTTON_PLUS = "buttonToggle8"
    const val PREF_BUTTON_MINUS = "buttonToggle9"
    const val PREF_BUTTON_DPAD = "buttonToggle10"
    const val PREF_STICK_L = "buttonToggle11"
    const val PREF_STICK_R = "buttonToggle12"
    const val PREF_BUTTON_STICK_L = "buttonToggle13"
    const val PREF_BUTTON_STICK_R = "buttonToggle14"
    const val PREF_BUTTON_HOME = "buttonToggle15"
    const val PREF_BUTTON_SCREENSHOT = "buttonToggle16"

    const val PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER = "EmulationMenuSettings_JoystickRelCenter"
    const val PREF_MENU_SETTINGS_DPAD_SLIDE = "EmulationMenuSettings_DpadSlideEnable"
    const val PREF_MENU_SETTINGS_HAPTICS = "EmulationMenuSettings_Haptics"
    const val PREF_MENU_SETTINGS_SHOW_FPS = "EmulationMenuSettings_ShowFps"
    const val PREF_MENU_SETTINGS_SHOW_OVERLAY = "EmulationMenuSettings_ShowOverlay"

    const val PREF_FIRST_APP_LAUNCH = "FirstApplicationLaunch"
    const val PREF_THEME = "Theme"
    const val PREF_THEME_MODE = "ThemeMode"
    const val PREF_BLACK_BACKGROUNDS = "BlackBackgrounds"

    val overlayPreferences = listOf(
        PREF_OVERLAY_VERSION,
        PREF_CONTROL_SCALE,
        PREF_CONTROL_OPACITY,
        PREF_TOUCH_ENABLED,
        PREF_BUTTON_A,
        PREF_BUTTON_B,
        PREF_BUTTON_X,
        PREF_BUTTON_Y,
        PREF_BUTTON_L,
        PREF_BUTTON_R,
        PREF_BUTTON_ZL,
        PREF_BUTTON_ZR,
        PREF_BUTTON_PLUS,
        PREF_BUTTON_MINUS,
        PREF_BUTTON_DPAD,
        PREF_STICK_L,
        PREF_STICK_R,
        PREF_BUTTON_HOME,
        PREF_BUTTON_SCREENSHOT,
        PREF_BUTTON_STICK_L,
        PREF_BUTTON_STICK_R
    )

    const val LayoutOption_Unspecified = 0
    const val LayoutOption_MobilePortrait = 4
    const val LayoutOption_MobileLandscape = 5
}
