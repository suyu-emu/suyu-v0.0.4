// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.content.SharedPreferences
import android.os.Build
import android.text.TextUtils
import android.widget.Toast
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.AbstractBooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.ByteSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.LongSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting
import org.yuzu.yuzu_emu.features.settings.model.view.*
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.SettingsViewModel

class SettingsFragmentPresenter(
    private val settingsViewModel: SettingsViewModel,
    private val adapter: SettingsAdapter,
    private var menuTag: String,
    private var gameId: String
) {
    private var settingsList = ArrayList<SettingsItem>()

    private val preferences: SharedPreferences
        get() = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    private val context: Context get() = YuzuApplication.appContext

    fun onViewCreated() {
        loadSettingsList()
    }

    private fun loadSettingsList() {
        if (!TextUtils.isEmpty(gameId)) {
            settingsViewModel.setToolbarTitle(
                context.getString(
                    R.string.advanced_settings_game,
                    gameId
                )
            )
        }

        val sl = ArrayList<SettingsItem>()
        when (menuTag) {
            SettingsFile.FILE_NAME_CONFIG -> addConfigSettings(sl)
            Settings.SECTION_GENERAL -> addGeneralSettings(sl)
            Settings.SECTION_SYSTEM -> addSystemSettings(sl)
            Settings.SECTION_RENDERER -> addGraphicsSettings(sl)
            Settings.SECTION_AUDIO -> addAudioSettings(sl)
            Settings.SECTION_THEME -> addThemeSettings(sl)
            Settings.SECTION_DEBUG -> addDebugSettings(sl)
            else -> {
                val context = YuzuApplication.appContext
                Toast.makeText(
                    context,
                    context.getString(R.string.unimplemented_menu),
                    Toast.LENGTH_SHORT
                ).show()
                return
            }
        }
        settingsList = sl
        adapter.setSettingsList(settingsList)
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        settingsViewModel.setToolbarTitle(context.getString(R.string.advanced_settings))
        sl.apply {
            add(SubmenuSetting(R.string.preferences_general, 0, Settings.SECTION_GENERAL))
            add(SubmenuSetting(R.string.preferences_system, 0, Settings.SECTION_SYSTEM))
            add(SubmenuSetting(R.string.preferences_graphics, 0, Settings.SECTION_RENDERER))
            add(SubmenuSetting(R.string.preferences_audio, 0, Settings.SECTION_AUDIO))
            add(SubmenuSetting(R.string.preferences_debug, 0, Settings.SECTION_DEBUG))
            add(
                RunnableSetting(R.string.reset_to_default, 0, false) {
                    settingsViewModel.setShouldShowResetSettingsDialog(true)
                }
            )
        }
    }

    private fun addGeneralSettings(sl: ArrayList<SettingsItem>) {
        settingsViewModel.setToolbarTitle(context.getString(R.string.preferences_general))
        sl.apply {
            add(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_SPEED_LIMIT,
                    R.string.frame_limit_enable,
                    R.string.frame_limit_enable_description
                )
            )
            add(
                SliderSetting(
                    ShortSetting.RENDERER_SPEED_LIMIT,
                    R.string.frame_limit_slider,
                    R.string.frame_limit_slider_description,
                    1,
                    200,
                    "%"
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.CPU_ACCURACY,
                    R.string.cpu_accuracy,
                    0,
                    R.array.cpuAccuracyNames,
                    R.array.cpuAccuracyValues
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.PICTURE_IN_PICTURE,
                    R.string.picture_in_picture,
                    R.string.picture_in_picture_description
                )
            )
        }
    }

    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        settingsViewModel.setToolbarTitle(context.getString(R.string.preferences_system))
        sl.apply {
            add(
                SwitchSetting(
                    BooleanSetting.USE_DOCKED_MODE,
                    R.string.use_docked_mode,
                    R.string.use_docked_mode_description
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.REGION_INDEX,
                    R.string.emulated_region,
                    0,
                    R.array.regionNames,
                    R.array.regionValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.LANGUAGE_INDEX,
                    R.string.emulated_language,
                    0,
                    R.array.languageNames,
                    R.array.languageValues
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.USE_CUSTOM_RTC,
                    R.string.use_custom_rtc,
                    R.string.use_custom_rtc_description
                )
            )
            add(DateTimeSetting(LongSetting.CUSTOM_RTC, R.string.set_custom_rtc, 0))
        }
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        settingsViewModel.setToolbarTitle(context.getString(R.string.preferences_graphics))
        sl.apply {
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ACCURACY,
                    R.string.renderer_accuracy,
                    0,
                    R.array.rendererAccuracyNames,
                    R.array.rendererAccuracyValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_RESOLUTION,
                    R.string.renderer_resolution,
                    0,
                    R.array.rendererResolutionNames,
                    R.array.rendererResolutionValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_VSYNC,
                    R.string.renderer_vsync,
                    0,
                    R.array.rendererVSyncNames,
                    R.array.rendererVSyncValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCALING_FILTER,
                    R.string.renderer_scaling_filter,
                    0,
                    R.array.rendererScalingFilterNames,
                    R.array.rendererScalingFilterValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ANTI_ALIASING,
                    R.string.renderer_anti_aliasing,
                    0,
                    R.array.rendererAntiAliasingNames,
                    R.array.rendererAntiAliasingValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCREEN_LAYOUT,
                    R.string.renderer_screen_layout,
                    0,
                    R.array.rendererScreenLayoutNames,
                    R.array.rendererScreenLayoutValues
                )
            )
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASPECT_RATIO,
                    R.string.renderer_aspect_ratio,
                    0,
                    R.array.rendererAspectRatioNames,
                    R.array.rendererAspectRatioValues
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE,
                    R.string.use_disk_shader_cache,
                    R.string.use_disk_shader_cache_description
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.RENDERER_FORCE_MAX_CLOCK,
                    R.string.renderer_force_max_clock,
                    R.string.renderer_force_max_clock_description
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS,
                    R.string.renderer_asynchronous_shaders,
                    R.string.renderer_asynchronous_shaders_description
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.RENDERER_REACTIVE_FLUSHING,
                    R.string.renderer_reactive_flushing,
                    R.string.renderer_reactive_flushing_description
                )
            )
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        settingsViewModel.setToolbarTitle(context.getString(R.string.preferences_audio))
        sl.apply {
            add(
                SingleChoiceSetting(
                    IntSetting.AUDIO_OUTPUT_ENGINE,
                    R.string.audio_output_engine,
                    0,
                    R.array.outputEngineEntries,
                    R.array.outputEngineValues
                )
            )
            add(
                SliderSetting(
                    ByteSetting.AUDIO_VOLUME,
                    R.string.audio_volume,
                    R.string.audio_volume_description,
                    0,
                    100,
                    "%"
                )
            )
        }
    }

    private fun addThemeSettings(sl: ArrayList<SettingsItem>) {
        settingsViewModel.setToolbarTitle(context.getString(R.string.preferences_theme))
        sl.apply {
            val theme: AbstractIntSetting = object : AbstractIntSetting {
                override val int: Int
                    get() = preferences.getInt(Settings.PREF_THEME, 0)

                override fun setInt(value: Int) {
                    preferences.edit()
                        .putInt(Settings.PREF_THEME, value)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String? = null
                override val category = Settings.Category.UiGeneral
                override val isRuntimeModifiable: Boolean = false
                override val defaultValue: Int = 0
                override fun reset() {
                    preferences.edit()
                        .putInt(Settings.PREF_THEME, defaultValue)
                        .apply()
                }
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
                override val int: Int
                    get() = preferences.getInt(Settings.PREF_THEME_MODE, -1)

                override fun setInt(value: Int) {
                    preferences.edit()
                        .putInt(Settings.PREF_THEME_MODE, value)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String? = null
                override val category = Settings.Category.UiGeneral
                override val isRuntimeModifiable: Boolean = false
                override val defaultValue: Int = -1
                override fun reset() {
                    preferences.edit()
                        .putInt(Settings.PREF_BLACK_BACKGROUNDS, defaultValue)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }
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
                override val boolean: Boolean
                    get() = preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false)

                override fun setBoolean(value: Boolean) {
                    preferences.edit()
                        .putBoolean(Settings.PREF_BLACK_BACKGROUNDS, value)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String? = null
                override val category = Settings.Category.UiGeneral
                override val isRuntimeModifiable: Boolean = false
                override val defaultValue: Boolean = false
                override fun reset() {
                    preferences.edit()
                        .putBoolean(Settings.PREF_BLACK_BACKGROUNDS, defaultValue)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }
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
        settingsViewModel.setToolbarTitle(context.getString(R.string.preferences_debug))
        sl.apply {
            add(HeaderSetting(R.string.gpu))
            add(
                SingleChoiceSetting(
                    IntSetting.RENDERER_BACKEND,
                    R.string.renderer_api,
                    0,
                    R.array.rendererApiNames,
                    R.array.rendererApiValues
                )
            )
            add(
                SwitchSetting(
                    BooleanSetting.RENDERER_DEBUG,
                    R.string.renderer_debug,
                    R.string.renderer_debug_description
                )
            )

            add(HeaderSetting(R.string.cpu))
            add(
                SwitchSetting(
                    BooleanSetting.CPU_DEBUG_MODE,
                    R.string.cpu_debug_mode,
                    R.string.cpu_debug_mode_description
                )
            )

            val fastmem = object : AbstractBooleanSetting {
                override val boolean: Boolean
                    get() =
                        BooleanSetting.FASTMEM.boolean && BooleanSetting.FASTMEM_EXCLUSIVES.boolean

                override fun setBoolean(value: Boolean) {
                    BooleanSetting.FASTMEM.setBoolean(value)
                    BooleanSetting.FASTMEM_EXCLUSIVES.setBoolean(value)
                }

                override val key: String? = null
                override val category = Settings.Category.Cpu
                override val isRuntimeModifiable: Boolean = false
                override val defaultValue: Boolean = true
                override fun reset() = setBoolean(defaultValue)
            }
            add(SwitchSetting(fastmem, R.string.fastmem, 0))
        }
    }
}
