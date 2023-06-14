// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import android.text.TextUtils
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivityView
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import java.util.*

class Settings {
    private var gameId: String? = null

    var isLoaded = false

    /**
     * A HashMap<String></String>, SettingSection> that constructs a new SettingSection instead of returning null
     * when getting a key not already in the map
     */
    class SettingsSectionMap : HashMap<String, SettingSection?>() {
        override operator fun get(key: String): SettingSection? {
            if (!super.containsKey(key)) {
                val section = SettingSection(key)
                super.put(key, section)
                return section
            }
            return super.get(key)
        }
    }

    var sections: HashMap<String, SettingSection?> = SettingsSectionMap()

    fun getSection(sectionName: String): SettingSection? {
        return sections[sectionName]
    }

    val isEmpty: Boolean
        get() = sections.isEmpty()

    fun loadSettings(view: SettingsActivityView? = null) {
        sections = SettingsSectionMap()
        loadYuzuSettings(view)
        if (!TextUtils.isEmpty(gameId)) {
            loadCustomGameSettings(gameId!!, view)
        }
        isLoaded = true
    }

    private fun loadYuzuSettings(view: SettingsActivityView?) {
        for ((fileName) in configFileSectionsMap) {
            sections.putAll(SettingsFile.readFile(fileName, view))
        }
    }

    private fun loadCustomGameSettings(gameId: String, view: SettingsActivityView?) {
        // Custom game settings
        mergeSections(SettingsFile.readCustomGameSettings(gameId, view))
    }

    private fun mergeSections(updatedSections: HashMap<String, SettingSection?>) {
        for ((key, updatedSection) in updatedSections) {
            if (sections.containsKey(key)) {
                val originalSection = sections[key]
                originalSection!!.mergeSection(updatedSection!!)
            } else {
                sections[key] = updatedSection
            }
        }
    }

    fun loadSettings(gameId: String, view: SettingsActivityView) {
        this.gameId = gameId
        loadSettings(view)
    }

    fun saveSettings(view: SettingsActivityView) {
        if (TextUtils.isEmpty(gameId)) {
            view.showToastMessage(
                YuzuApplication.appContext.getString(R.string.ini_saved),
                false
            )

            for ((fileName, sectionNames) in configFileSectionsMap) {
                val iniSections = TreeMap<String, SettingSection>()
                for (section in sectionNames) {
                    iniSections[section] = sections[section]!!
                }

                SettingsFile.saveFile(fileName, iniSections, view)
            }
        } else {
            // Custom game settings
            view.showToastMessage(
                YuzuApplication.appContext.getString(R.string.gameid_saved, gameId),
                false
            )

            SettingsFile.saveCustomGameSettings(gameId, sections)
        }
    }

    companion object {
        const val SECTION_GENERAL = "General"
        const val SECTION_SYSTEM = "System"
        const val SECTION_RENDERER = "Renderer"
        const val SECTION_AUDIO = "Audio"
        const val SECTION_CPU = "Cpu"
        const val SECTION_THEME = "Theme"
        const val SECTION_DEBUG = "Debug"

        const val PREF_OVERLAY_INIT = "OverlayInit"
        const val PREF_CONTROL_SCALE = "controlScale"
        const val PREF_CONTROL_OPACITY = "controlOpacity"
        const val PREF_TOUCH_ENABLED = "isTouchEnabled"
        const val PREF_BUTTON_TOGGLE_0 = "buttonToggle0"
        const val PREF_BUTTON_TOGGLE_1 = "buttonToggle1"
        const val PREF_BUTTON_TOGGLE_2 = "buttonToggle2"
        const val PREF_BUTTON_TOGGLE_3 = "buttonToggle3"
        const val PREF_BUTTON_TOGGLE_4 = "buttonToggle4"
        const val PREF_BUTTON_TOGGLE_5 = "buttonToggle5"
        const val PREF_BUTTON_TOGGLE_6 = "buttonToggle6"
        const val PREF_BUTTON_TOGGLE_7 = "buttonToggle7"
        const val PREF_BUTTON_TOGGLE_8 = "buttonToggle8"
        const val PREF_BUTTON_TOGGLE_9 = "buttonToggle9"
        const val PREF_BUTTON_TOGGLE_10 = "buttonToggle10"
        const val PREF_BUTTON_TOGGLE_11 = "buttonToggle11"
        const val PREF_BUTTON_TOGGLE_12 = "buttonToggle12"
        const val PREF_BUTTON_TOGGLE_13 = "buttonToggle13"
        const val PREF_BUTTON_TOGGLE_14 = "buttonToggle14"

        const val PREF_MENU_SETTINGS_JOYSTICK_REL_CENTER = "EmulationMenuSettings_JoystickRelCenter"
        const val PREF_MENU_SETTINGS_DPAD_SLIDE = "EmulationMenuSettings_DpadSlideEnable"
        const val PREF_MENU_SETTINGS_HAPTICS = "EmulationMenuSettings_Haptics"
        const val PREF_MENU_SETTINGS_SHOW_FPS = "EmulationMenuSettings_ShowFps"
        const val PREF_MENU_SETTINGS_SHOW_OVERLAY = "EmulationMenuSettings_ShowOverlay"

        const val PREF_FIRST_APP_LAUNCH = "FirstApplicationLaunch"
        const val PREF_THEME = "Theme"
        const val PREF_THEME_MODE = "ThemeMode"
        const val PREF_BLACK_BACKGROUNDS = "BlackBackgrounds"

        private val configFileSectionsMap: MutableMap<String, List<String>> = HashMap()

        const val LayoutOption_Unspecified = 0
        const val LayoutOption_MobilePortrait = 4
        const val LayoutOption_MobileLandscape = 5

        init {
            configFileSectionsMap[SettingsFile.FILE_NAME_CONFIG] =
                listOf(
                    SECTION_GENERAL,
                    SECTION_SYSTEM,
                    SECTION_RENDERER,
                    SECTION_AUDIO,
                    SECTION_CPU
                )
        }
    }
}
