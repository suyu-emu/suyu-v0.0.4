// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.SharedPreferences
import android.os.Build
import android.widget.Toast
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
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
import org.yuzu.yuzu_emu.model.SettingsViewModel
import org.yuzu.yuzu_emu.utils.NativeConfig

class SettingsFragmentPresenter(
    private val settingsViewModel: SettingsViewModel,
    private val adapter: SettingsAdapter,
    private var menuTag: Settings.MenuTag
) {
    private var settingsList = ArrayList<SettingsItem>()

    private val preferences: SharedPreferences
        get() = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    // Extension for altering settings list based on each setting's properties
    fun ArrayList<SettingsItem>.add(key: String) {
        val item = SettingsItem.settingsItems[key]!!
        if (settingsViewModel.game != null && !item.setting.isSwitchable) {
            return
        }

        if (!NativeConfig.isPerGameConfigLoaded() && !NativeLibrary.isRunning()) {
            item.setting.global = true
        }

        val pairedSettingKey = item.setting.pairedSettingKey
        if (pairedSettingKey.isNotEmpty()) {
            val pairedSettingValue = NativeConfig.getBoolean(
                pairedSettingKey,
                if (NativeLibrary.isRunning() && !NativeConfig.isPerGameConfigLoaded()) {
                    !NativeConfig.usingGlobal(pairedSettingKey)
                } else {
                    NativeConfig.usingGlobal(pairedSettingKey)
                }
            )
            if (!pairedSettingValue) return
        }
        add(item)
    }

    fun onViewCreated() {
        loadSettingsList()
    }

    fun loadSettingsList() {
        val sl = ArrayList<SettingsItem>()
        when (menuTag) {
            Settings.MenuTag.SECTION_ROOT -> addConfigSettings(sl)
            Settings.MenuTag.SECTION_SYSTEM -> addSystemSettings(sl)
            Settings.MenuTag.SECTION_RENDERER -> addGraphicsSettings(sl)
            Settings.MenuTag.SECTION_AUDIO -> addAudioSettings(sl)
            Settings.MenuTag.SECTION_THEME -> addThemeSettings(sl)
            Settings.MenuTag.SECTION_DEBUG -> addDebugSettings(sl)
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
        adapter.submitList(settingsList)
    }

    private fun addConfigSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(
                SubmenuSetting(
                    R.string.preferences_system,
                    R.string.preferences_system_description,
                    R.drawable.ic_system_settings,
                    Settings.MenuTag.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_graphics,
                    R.string.preferences_graphics_description,
                    R.drawable.ic_graphics,
                    Settings.MenuTag.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_audio,
                    R.string.preferences_audio_description,
                    R.drawable.ic_audio,
                    Settings.MenuTag.SECTION_AUDIO
                )
            )
            add(
                SubmenuSetting(
                    R.string.preferences_debug,
                    R.string.preferences_debug_description,
                    R.drawable.ic_code,
                    Settings.MenuTag.SECTION_DEBUG
                )
            )
            add(
                RunnableSetting(
                    R.string.reset_to_default,
                    R.string.reset_to_default_description,
                    false,
                    R.drawable.ic_restore
                ) { settingsViewModel.setShouldShowResetSettingsDialog(true) }
            )
        }
    }

    private fun addSystemSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(BooleanSetting.RENDERER_USE_SPEED_LIMIT.key)
            add(ShortSetting.RENDERER_SPEED_LIMIT.key)
            add(BooleanSetting.USE_DOCKED_MODE.key)
            add(IntSetting.REGION_INDEX.key)
            add(IntSetting.LANGUAGE_INDEX.key)
            add(BooleanSetting.USE_CUSTOM_RTC.key)
            add(LongSetting.CUSTOM_RTC.key)
        }
    }

    private fun addGraphicsSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(IntSetting.RENDERER_ACCURACY.key)
            add(IntSetting.RENDERER_RESOLUTION.key)
            add(IntSetting.RENDERER_VSYNC.key)
            add(IntSetting.RENDERER_SCALING_FILTER.key)
            add(IntSetting.RENDERER_ANTI_ALIASING.key)
            add(IntSetting.MAX_ANISOTROPY.key)
            add(IntSetting.RENDERER_SCREEN_LAYOUT.key)
            add(IntSetting.RENDERER_ASPECT_RATIO.key)
            add(BooleanSetting.PICTURE_IN_PICTURE.key)
            add(BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE.key)
            add(BooleanSetting.RENDERER_FORCE_MAX_CLOCK.key)
            add(BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS.key)
            add(BooleanSetting.RENDERER_REACTIVE_FLUSHING.key)
        }
    }

    private fun addAudioSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            add(IntSetting.AUDIO_OUTPUT_ENGINE.key)
            add(ByteSetting.AUDIO_VOLUME.key)
        }
    }

    private fun addThemeSettings(sl: ArrayList<SettingsItem>) {
        sl.apply {
            val theme: AbstractIntSetting = object : AbstractIntSetting {
                override fun getInt(needsGlobal: Boolean): Int =
                    preferences.getInt(Settings.PREF_THEME, 0)

                override fun setInt(value: Int) {
                    preferences.edit()
                        .putInt(Settings.PREF_THEME, value)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String = Settings.PREF_THEME
                override val isRuntimeModifiable: Boolean = false
                override fun getValueAsString(needsGlobal: Boolean): String = getInt().toString()
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
                override fun getInt(needsGlobal: Boolean): Int =
                    preferences.getInt(Settings.PREF_THEME_MODE, -1)

                override fun setInt(value: Int) {
                    preferences.edit()
                        .putInt(Settings.PREF_THEME_MODE, value)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String = Settings.PREF_THEME_MODE
                override val isRuntimeModifiable: Boolean = false
                override fun getValueAsString(needsGlobal: Boolean): String = getInt().toString()
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
                override fun getBoolean(needsGlobal: Boolean): Boolean =
                    preferences.getBoolean(Settings.PREF_BLACK_BACKGROUNDS, false)

                override fun setBoolean(value: Boolean) {
                    preferences.edit()
                        .putBoolean(Settings.PREF_BLACK_BACKGROUNDS, value)
                        .apply()
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String = Settings.PREF_BLACK_BACKGROUNDS
                override val isRuntimeModifiable: Boolean = false
                override fun getValueAsString(needsGlobal: Boolean): String =
                    getBoolean().toString()

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
        sl.apply {
            add(HeaderSetting(R.string.gpu))
            add(IntSetting.RENDERER_BACKEND.key)
            add(BooleanSetting.RENDERER_DEBUG.key)

            add(HeaderSetting(R.string.cpu))
            add(IntSetting.CPU_BACKEND.key)
            add(IntSetting.CPU_ACCURACY.key)
            add(BooleanSetting.CPU_DEBUG_MODE.key)
            add(SettingsItem.FASTMEM_COMBINED)
        }
    }
}
