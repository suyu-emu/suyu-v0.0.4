// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.SharedPreferences
import android.os.Build
import android.text.TextUtils
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile

class SettingsFragmentPresenter(private val fragmentView: SettingsFragmentView) {
    private var menuTag: String? = null
    private lateinit var gameId: String
    private var settingsList: ArrayList<SettingsItem>? = null

    private val settingsActivity get() = fragmentView.activityView as AppCompatActivity
    private val settings get() = fragmentView.activityView!!.settings

    private lateinit var preferences: SharedPreferences

    fun onCreate(menuTag: String, gameId: String) {
        this.gameId = gameId
        this.menuTag = menuTag
    }

    fun onViewCreated() {
        preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        loadSettingsList()
    }

    fun putSetting(setting: AbstractSetting) {
        if (setting.section == null) {
            return
        }

        val section = settings.getSection(setting.section!!)!!
        if (section.getSetting(setting.key!!) == null) {
            section.putSetting(setting)
        }
    }

    fun loadSettingsList() {
        if (!TextUtils.isEmpty(gameId)) {
            settingsActivity.title = "Game Settings: $gameId"
        }
        val sl = ArrayList<SettingsItem>()
        if (menuTag == null) {
            return
        }
        when (menuTag) {
            SettingsFile.FILE_NAME_CONFIG -> addConfigSettings(sl)
            Settings.SECTION_GENERAL -> addGeneralSettings(sl)
            Settings.SECTION_SYSTEM -> addSystemSettings(sl)
            Settings.SECTION_RENDERER -> addGraphicsSettings(sl)
            Settings.SECTION_AUDIO -> addAudioSettings(sl)
            Settings.SECTION_THEME -> addThemeSettings(sl)
            else -> {
                fragmentView.showToastMessage("Unimplemented menu", false)
                return
            }
        }
        settingsList = sl
        fragmentView.showSettingsList(settingsList!!)
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setTitle(R.string.preferences_settings)
        sl.apply {
            add(
                SubmenuSetting(
                    null,
                    R.string.preferences_general,
                    0,
                    Settings.SECTION_GENERAL
                )
            )
            add(
                SubmenuSetting(
                    null,
                    R.string.preferences_system,
                    0,
                    Settings.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    null,
                    R.string.preferences_graphics,
                    0,
                    Settings.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    null,
                    R.string.preferences_audio,
                    0,
                    Settings.SECTION_AUDIO
                )
            )
            add(
                SubmenuSetting(
                    null,
                    R.string.preferences_theme,
                    0,
                    Settings.SECTION_THEME
                )
            )
        }
    }

    private fun addGeneralSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setTitle(R.string.preferences_general)
        sl.apply {
            add(
                SwitchSetting(
                    IntSetting.RENDERER_USE_SPEED_LIMIT,
                    R.string.frame_limit_enable,
                    R.string.frame_limit_enable_description,
                    IntSetting.RENDERER_USE_SPEED_LIMIT.key,
                    true
                )
            )
            add(
                SliderSetting(
                    IntSetting.RENDERER_SPEED_LIMIT,
                    R.string.frame_limit_slider,
                    R.string.frame_limit_slider_description,
                    1,
                    200,
                    "%",
                    IntSetting.RENDERER_SPEED_LIMIT.key,
                    100
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.CPU_ACCURACY,
                    R.string.cpu_accuracy,
                    0,
                    R.array.cpuAccuracyNames,
                    R.array.cpuAccuracyValues,
                    IntSetting.CPU_ACCURACY.key,
                    0
                )
            )
        }
    }

    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setTitle(R.string.preferences_system)
        sl.apply {
            add(
                SwitchSetting(
                    IntSetting.USE_DOCKED_MODE,
                    R.string.use_docked_mode,
                    R.string.use_docked_mode_description,
                    IntSetting.USE_DOCKED_MODE.key,
                    false
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.REGION_INDEX,
                    R.string.emulated_region,
                    0,
                    R.array.regionNames,
                    R.array.regionValues,
                    IntSetting.REGION_INDEX.key,
                    -1
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.LANGUAGE_INDEX,
                    R.string.emulated_language,
                    0,
                    R.array.languageNames,
                    R.array.languageValues,
                    IntSetting.LANGUAGE_INDEX.key,
                    1
                )
            )
        }
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setTitle(R.string.preferences_graphics)
        sl.apply {
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_BACKEND,
                    R.string.renderer_api,
                    0,
                    R.array.rendererApiNames,
                    R.array.rendererApiValues,
                    IntSetting.RENDERER_BACKEND.key,
                    1
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ACCURACY,
                    R.string.renderer_accuracy,
                    0,
                    R.array.rendererAccuracyNames,
                    R.array.rendererAccuracyValues,
                    IntSetting.RENDERER_ACCURACY.key,
                    0
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_RESOLUTION,
                    R.string.renderer_resolution,
                    0,
                    R.array.rendererResolutionNames,
                    R.array.rendererResolutionValues,
                    IntSetting.RENDERER_RESOLUTION.key,
                    2
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCALING_FILTER,
                    R.string.renderer_scaling_filter,
                    0,
                    R.array.rendererScalingFilterNames,
                    R.array.rendererScalingFilterValues,
                    IntSetting.RENDERER_SCALING_FILTER.key,
                    1
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ANTI_ALIASING,
                    R.string.renderer_anti_aliasing,
                    0,
                    R.array.rendererAntiAliasingNames,
                    R.array.rendererAntiAliasingValues,
                    IntSetting.RENDERER_ANTI_ALIASING.key,
                    0
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASPECT_RATIO,
                    R.string.renderer_aspect_ratio,
                    0,
                    R.array.rendererAspectRatioNames,
                    R.array.rendererAspectRatioValues,
                    IntSetting.RENDERER_ASPECT_RATIO.key,
                    0
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_USE_DISK_SHADER_CACHE,
                    R.string.use_disk_shader_cache,
                    R.string.use_disk_shader_cache_description,
                    IntSetting.RENDERER_USE_DISK_SHADER_CACHE.key,
                    true
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_FORCE_MAX_CLOCK,
                    R.string.renderer_force_max_clock,
                    R.string.renderer_force_max_clock_description,
                    IntSetting.RENDERER_FORCE_MAX_CLOCK.key,
                    true
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_ASYNCHRONOUS_SHADERS,
                    R.string.renderer_asynchronous_shaders,
                    R.string.renderer_asynchronous_shaders_description,
                    IntSetting.RENDERER_ASYNCHRONOUS_SHADERS.key,
                    false
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_DEBUG,
                    R.string.renderer_debug,
                    R.string.renderer_debug_description,
                    IntSetting.RENDERER_DEBUG.key,
                    false
                )
            )
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setTitle(R.string.preferences_audio)
        sl.add(
            SliderSetting(
                IntSetting.AUDIO_VOLUME,
                R.string.audio_volume,
                R.string.audio_volume_description,
                0,
                100,
                "%",
                IntSetting.AUDIO_VOLUME.key,
                100
            )
        )
    }

    private fun addThemeSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setTitle(R.string.preferences_theme)
        sl.apply {
            val theme: AbstractIntSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = preferences.getInt(Settings.PREF_THEME, 0)
                    set(value) {
                        preferences.edit().putInt(Settings.PREF_THEME, value).apply()
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = true
                override val valueAsString: String
                    get() = preferences.getInt(Settings.PREF_THEME, 0).toString()
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                add(
                    SingleChoiceSetting(
                        theme,
                        R.string.change_app_theme,
                        0,
                        R.array.themeEntriesA12,
                        R.array.themeValuesA12
                    )
                )
            } else {
                add(
                    SingleChoiceSetting(
                        theme,
                        R.string.change_app_theme,
                        0,
                        R.array.themeEntries,
                        R.array.themeValues
                    )
                )
            }
        }
    }
}
