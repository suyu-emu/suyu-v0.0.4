// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.AbstractBooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.ByteSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.LongSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a [AbstractSetting] object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
abstract class SettingsItem(
    val setting: AbstractSetting,
    val nameId: Int,
    val descriptionId: Int
) {
    abstract val type: Int

    val isEditable: Boolean
        get() {
            if (!NativeLibrary.isRunning()) return true
            return setting.isRuntimeModifiable
        }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_STRING_SINGLE_CHOICE = 5
        const val TYPE_DATETIME_SETTING = 6
        const val TYPE_RUNNABLE = 7

        const val FASTMEM_COMBINED = "fastmem_combined"

        val emptySetting = object : AbstractSetting {
            override val key: String = ""
            override val category: Settings.Category = Settings.Category.Ui
            override val defaultValue: Any = false
            override fun reset() {}
        }

        // Extension for putting SettingsItems into a hashmap without repeating yourself
        fun HashMap<String, SettingsItem>.put(item: SettingsItem) {
            put(item.setting.key, item)
        }

        // List of all general
        val settingsItems = HashMap<String, SettingsItem>().apply {
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_SPEED_LIMIT,
                    R.string.frame_limit_enable,
                    R.string.frame_limit_enable_description
                )
            )
            put(
                SliderSetting(
                    ShortSetting.RENDERER_SPEED_LIMIT,
                    R.string.frame_limit_slider,
                    R.string.frame_limit_slider_description,
                    1,
                    400,
                    "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.CPU_BACKEND,
                    R.string.cpu_backend,
                    0,
                    R.array.cpuBackendArm64Names,
                    R.array.cpuBackendArm64Values
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.CPU_ACCURACY,
                    R.string.cpu_accuracy,
                    0,
                    R.array.cpuAccuracyNames,
                    R.array.cpuAccuracyValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.PICTURE_IN_PICTURE,
                    R.string.picture_in_picture,
                    R.string.picture_in_picture_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_DOCKED_MODE,
                    R.string.use_docked_mode,
                    R.string.use_docked_mode_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.REGION_INDEX,
                    R.string.emulated_region,
                    0,
                    R.array.regionNames,
                    R.array.regionValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.LANGUAGE_INDEX,
                    R.string.emulated_language,
                    0,
                    R.array.languageNames,
                    R.array.languageValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_CUSTOM_RTC,
                    R.string.use_custom_rtc,
                    R.string.use_custom_rtc_description
                )
            )
            put(DateTimeSetting(LongSetting.CUSTOM_RTC, R.string.set_custom_rtc, 0))
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ACCURACY,
                    R.string.renderer_accuracy,
                    0,
                    R.array.rendererAccuracyNames,
                    R.array.rendererAccuracyValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_RESOLUTION,
                    R.string.renderer_resolution,
                    0,
                    R.array.rendererResolutionNames,
                    R.array.rendererResolutionValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_VSYNC,
                    R.string.renderer_vsync,
                    0,
                    R.array.rendererVSyncNames,
                    R.array.rendererVSyncValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCALING_FILTER,
                    R.string.renderer_scaling_filter,
                    0,
                    R.array.rendererScalingFilterNames,
                    R.array.rendererScalingFilterValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ANTI_ALIASING,
                    R.string.renderer_anti_aliasing,
                    0,
                    R.array.rendererAntiAliasingNames,
                    R.array.rendererAntiAliasingValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCREEN_LAYOUT,
                    R.string.renderer_screen_layout,
                    0,
                    R.array.rendererScreenLayoutNames,
                    R.array.rendererScreenLayoutValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASPECT_RATIO,
                    R.string.renderer_aspect_ratio,
                    0,
                    R.array.rendererAspectRatioNames,
                    R.array.rendererAspectRatioValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE,
                    R.string.use_disk_shader_cache,
                    R.string.use_disk_shader_cache_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_FORCE_MAX_CLOCK,
                    R.string.renderer_force_max_clock,
                    R.string.renderer_force_max_clock_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS,
                    R.string.renderer_asynchronous_shaders,
                    R.string.renderer_asynchronous_shaders_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_REACTIVE_FLUSHING,
                    R.string.renderer_reactive_flushing,
                    R.string.renderer_reactive_flushing_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.AUDIO_OUTPUT_ENGINE,
                    R.string.audio_output_engine,
                    0,
                    R.array.outputEngineEntries,
                    R.array.outputEngineValues
                )
            )
            put(
                SliderSetting(
                    ByteSetting.AUDIO_VOLUME,
                    R.string.audio_volume,
                    R.string.audio_volume_description,
                    0,
                    100,
                    "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_BACKEND,
                    R.string.renderer_api,
                    0,
                    R.array.rendererApiNames,
                    R.array.rendererApiValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_DEBUG,
                    R.string.renderer_debug,
                    R.string.renderer_debug_description
                )
            )
            put(
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

                override val key: String = FASTMEM_COMBINED
                override val category = Settings.Category.Cpu
                override val isRuntimeModifiable: Boolean = false
                override val defaultValue: Boolean = true
                override fun reset() = setBoolean(defaultValue)
            }
            put(SwitchSetting(fastmem, R.string.fastmem, 0))
        }
    }
}
