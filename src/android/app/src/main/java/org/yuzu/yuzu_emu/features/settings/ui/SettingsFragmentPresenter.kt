// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.SharedPreferences
import android.os.Build
import android.text.TextUtils
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.AbstractBooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.fragments.ResetSettingsDialogFragment
import org.yuzu.yuzu_emu.utils.ThemeHelper

class SettingsFragmentPresenter(private val fragmentView: SettingsFragmentView) {
    private var menuTag: String? = null
    private lateinit var gameId: String
    private var settingsList: ArrayList<SettingsItem>? = null

    private val settingsActivity get() = fragmentView.activityView as SettingsActivity
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
            settingsActivity.setToolbarTitle("Game Settings: $gameId")
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
            Settings.SECTION_DEBUG -> addDebugSettings(sl)
            else -> {
                fragmentView.showToastMessage("Unimplemented menu", false)
                return
            }
        }
        settingsList = sl
        fragmentView.showSettingsList(settingsList!!)
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.advanced_settings))
        sl.apply {
            add(
                SubmenuSetting(
                    R.string.preferences_general,
                    0,
                    Settings.SECTION_GENERAL
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_system,
                    0,
                    Settings.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_graphics,
                    0,
                    Settings.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_audio,
                    0,
                    Settings.SECTION_AUDIO
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_debug,
                    0,
                    Settings.SECTION_DEBUG
                )
            )
            add(
                RunnableSetting(
                    R.string.reset_to_default,
                    0,
                    false
                ) {
                    ResetSettingsDialogFragment().show(
                        settingsActivity.supportFragmentManager,
                        ResetSettingsDialogFragment.TAG
                    )
                }
            )
        }
    }

    private fun addGeneralSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_general))
        sl.apply {
            add(
                SwitchSetting(
                    IntSetting.RENDERER_USE_SPEED_LIMIT,
                    R.string.frame_limit_enable,
                    R.string.frame_limit_enable_description,
                    IntSetting.RENDERER_USE_SPEED_LIMIT.key,
                    IntSetting.RENDERER_USE_SPEED_LIMIT.defaultValue
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
                    IntSetting.RENDERER_SPEED_LIMIT.defaultValue
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
                    IntSetting.CPU_ACCURACY.defaultValue
                )
            )
        }
    }

    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_system))
        sl.apply {
            add(
                SwitchSetting(
                    IntSetting.USE_DOCKED_MODE,
                    R.string.use_docked_mode,
                    R.string.use_docked_mode_description,
                    IntSetting.USE_DOCKED_MODE.key,
                    IntSetting.USE_DOCKED_MODE.defaultValue
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
                    IntSetting.REGION_INDEX.defaultValue
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
                    IntSetting.LANGUAGE_INDEX.defaultValue
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.USE_CUSTOM_RTC,
                    R.string.use_custom_rtc,
                    R.string.use_custom_rtc_description,
                    BooleanSetting.USE_CUSTOM_RTC.key,
                    BooleanSetting.USE_CUSTOM_RTC.defaultValue
                )
            )
            add(
                DateTimeSetting(
                    StringSetting.CUSTOM_RTC,
                    R.string.set_custom_rtc,
                    0,
                    StringSetting.CUSTOM_RTC.key,
                    StringSetting.CUSTOM_RTC.defaultValue
                )
            )
        }
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_graphics))
        sl.apply {

            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ACCURACY,
                    R.string.renderer_accuracy,
                    0,
                    R.array.rendererAccuracyNames,
                    R.array.rendererAccuracyValues,
                    IntSetting.RENDERER_ACCURACY.key,
                    IntSetting.RENDERER_ACCURACY.defaultValue
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
                    IntSetting.RENDERER_RESOLUTION.defaultValue
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_VSYNC,
                    R.string.renderer_vsync,
                    0,
                    R.array.rendererVSyncNames,
                    R.array.rendererVSyncValues,
                    IntSetting.RENDERER_VSYNC.key,
                    IntSetting.RENDERER_VSYNC.defaultValue
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
                    IntSetting.RENDERER_SCALING_FILTER.defaultValue
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
                    IntSetting.RENDERER_ANTI_ALIASING.defaultValue
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
                    IntSetting.RENDERER_ASPECT_RATIO.defaultValue
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_USE_DISK_SHADER_CACHE,
                    R.string.use_disk_shader_cache,
                    R.string.use_disk_shader_cache_description,
                    IntSetting.RENDERER_USE_DISK_SHADER_CACHE.key,
                    IntSetting.RENDERER_USE_DISK_SHADER_CACHE.defaultValue
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_FORCE_MAX_CLOCK,
                    R.string.renderer_force_max_clock,
                    R.string.renderer_force_max_clock_description,
                    IntSetting.RENDERER_FORCE_MAX_CLOCK.key,
                    IntSetting.RENDERER_FORCE_MAX_CLOCK.defaultValue
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_ASYNCHRONOUS_SHADERS,
                    R.string.renderer_asynchronous_shaders,
                    R.string.renderer_asynchronous_shaders_description,
                    IntSetting.RENDERER_ASYNCHRONOUS_SHADERS.key,
                    IntSetting.RENDERER_ASYNCHRONOUS_SHADERS.defaultValue
                )
            )
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_audio))
        sl.add(
            SliderSetting(
                IntSetting.AUDIO_VOLUME,
                R.string.audio_volume,
                R.string.audio_volume_description,
                0,
                100,
                "%",
                IntSetting.AUDIO_VOLUME.key,
                IntSetting.AUDIO_VOLUME.defaultValue
            )
        )
    }

    private fun addThemeSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_theme))
        sl.apply {
            val theme: AbstractIntSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = preferences.getInt(Settings.PREF_THEME, 0)
                    set(value) {
                        preferences.edit()
                            .putInt(Settings.PREF_THEME, value)
                            .apply()
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getInt(Settings.PREF_THEME, 0).toString()
                override val defaultValue: Any = 0
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

            val themeMode: AbstractIntSetting = object : AbstractIntSetting {
                override var int: Int
                    get() = preferences.getInt(Settings.PREF_THEME_MODE, -1)
                    set(value) {
                        preferences.edit()
                            .putInt(Settings.PREF_THEME_MODE, value)
                            .apply()
                        ThemeHelper.setThemeMode(settingsActivity)
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getInt(Settings.PREF_THEME_MODE, -1).toString()
                override val defaultValue: Any = -1
            }

            add(
                SingleChoiceSetting(
                    themeMode,
                    R.string.change_theme_mode,
                    0,
                    R.array.themeModeEntries,
                    R.array.themeModeValues
                )
            )

            val blackBackgrounds: AbstractBooleanSetting = object : AbstractBooleanSetting {
                override var boolean: Boolean
                    get() = preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false)
                    set(value) {
                        preferences.edit()
                            .putBoolean(Settings.PREF_BLACK_BACKGROUNDS, value)
                            .apply()
                        settingsActivity.recreate()
                    }
                override val key: String? = null
                override val section: String? = null
                override val isRuntimeEditable: Boolean = false
                override val valueAsString: String
                    get() = preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false)
                        .toString()
                override val defaultValue: Any = false
            }

            add(
                SwitchSetting(
                    blackBackgrounds,
                    R.string.use_black_backgrounds,
                    R.string.use_black_backgrounds_description
                )
            )
        }
    }

    private fun addDebugSettings(sl: ArrayList<SettingsItem>) {
        settingsActivity.setToolbarTitle(settingsActivity.getString(R.string.preferences_debug))
        sl.apply {
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_BACKEND,
                    R.string.renderer_api,
                    0,
                    R.array.rendererApiNames,
                    R.array.rendererApiValues,
                    IntSetting.RENDERER_BACKEND.key,
                    IntSetting.RENDERER_BACKEND.defaultValue
                )
            )
            add(
                SwitchSetting(
                    IntSetting.RENDERER_DEBUG,
                    R.string.renderer_debug,
                    R.string.renderer_debug_description,
                    IntSetting.RENDERER_DEBUG.key,
                    IntSetting.RENDERER_DEBUG.defaultValue
                )
            )
        }
    }
}
