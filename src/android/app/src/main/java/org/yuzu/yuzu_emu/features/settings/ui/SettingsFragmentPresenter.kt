// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.os.Build
import android.widget.Toast
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
                    titleId = R.string.preferences_system,
                    descriptionId = R.string.preferences_system_description,
                    iconId = R.drawable.ic_system_settings,
                    menuKey = MenuTag.SECTION_SYSTEM
                )
            )
            add(
                SubmenuSetting(
                    titleId = R.string.preferences_graphics,
                    descriptionId = R.string.preferences_graphics_description,
                    iconId = R.drawable.ic_graphics,
                    menuKey = MenuTag.SECTION_RENDERER
                )
            )
            add(
                SubmenuSetting(
                    titleId = R.string.preferences_audio,
                    descriptionId = R.string.preferences_audio_description,
                    iconId = R.drawable.ic_audio,
                    menuKey = MenuTag.SECTION_AUDIO
                )
            )
            add(
                SubmenuSetting(
                    titleId = R.string.preferences_debug,
                    descriptionId = R.string.preferences_debug_description,
                    iconId = R.drawable.ic_code,
                    menuKey = MenuTag.SECTION_DEBUG
                )
            )
            add(
                RunnableSetting(
                    titleId = R.string.reset_to_default,
                    descriptionId = R.string.reset_to_default_description,
                    iconId = R.drawable.ic_restore
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
            add(IntSetting.FSR_SHARPENING_SLIDER.key)
            add(IntSetting.RENDERER_ANTI_ALIASING.key)
            add(IntSetting.MAX_ANISOTROPY.key)
            add(IntSetting.RENDERER_SCREEN_LAYOUT.key)
            add(IntSetting.RENDERER_ASPECT_RATIO.key)
            add(IntSetting.VERTICAL_ALIGNMENT.key)
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
                override fun getInt(needsGlobal: Boolean): Int = IntSetting.THEME.getInt()
                override fun setInt(value: Int) {
                    IntSetting.THEME.setInt(value)
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String = IntSetting.THEME.key
                override val isRuntimeModifiable: Boolean = IntSetting.THEME.isRuntimeModifiable
                override fun getValueAsString(needsGlobal: Boolean): String =
                    IntSetting.THEME.getValueAsString()

                override val defaultValue: Int = IntSetting.THEME.defaultValue
                override fun reset() = IntSetting.THEME.setInt(defaultValue)
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                add(
                    SingleChoiceSetting(
                        theme,
                        titleId = R.string.change_app_theme,
                        choicesId = R.array.themeEntriesA12,
                        valuesId = R.array.themeValuesA12
                    )
                )
            } else {
                add(
                    SingleChoiceSetting(
                        theme,
                        titleId = R.string.change_app_theme,
                        choicesId = R.array.themeEntries,
                        valuesId = R.array.themeValues
                    )
                )
            }

            val themeMode: AbstractIntSetting = object : AbstractIntSetting {
                override fun getInt(needsGlobal: Boolean): Int = IntSetting.THEME_MODE.getInt()
                override fun setInt(value: Int) {
                    IntSetting.THEME_MODE.setInt(value)
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String = IntSetting.THEME_MODE.key
                override val isRuntimeModifiable: Boolean =
                    IntSetting.THEME_MODE.isRuntimeModifiable

                override fun getValueAsString(needsGlobal: Boolean): String =
                    IntSetting.THEME_MODE.getValueAsString()

                override val defaultValue: Int = IntSetting.THEME_MODE.defaultValue
                override fun reset() {
                    IntSetting.THEME_MODE.setInt(defaultValue)
                    settingsViewModel.setShouldRecreate(true)
                }
            }

            add(
                SingleChoiceSetting(
                    themeMode,
                    titleId = R.string.change_theme_mode,
                    choicesId = R.array.themeModeEntries,
                    valuesId = R.array.themeModeValues
                )
            )

            val blackBackgrounds: AbstractBooleanSetting = object : AbstractBooleanSetting {
                override fun getBoolean(needsGlobal: Boolean): Boolean =
                    BooleanSetting.BLACK_BACKGROUNDS.getBoolean()

                override fun setBoolean(value: Boolean) {
                    BooleanSetting.BLACK_BACKGROUNDS.setBoolean(value)
                    settingsViewModel.setShouldRecreate(true)
                }

                override val key: String = BooleanSetting.BLACK_BACKGROUNDS.key
                override val isRuntimeModifiable: Boolean =
                    BooleanSetting.BLACK_BACKGROUNDS.isRuntimeModifiable

                override fun getValueAsString(needsGlobal: Boolean): String =
                    BooleanSetting.BLACK_BACKGROUNDS.getValueAsString()

                override val defaultValue: Boolean = BooleanSetting.BLACK_BACKGROUNDS.defaultValue
                override fun reset() {
                    BooleanSetting.BLACK_BACKGROUNDS
                        .setBoolean(BooleanSetting.BLACK_BACKGROUNDS.defaultValue)
                    settingsViewModel.setShouldRecreate(true)
                }
            }

            add(
                SwitchSetting(
                    blackBackgrounds,
                    titleId = R.string.use_black_backgrounds,
                    descriptionId = R.string.use_black_backgrounds_description
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
