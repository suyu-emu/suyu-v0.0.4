// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.StringRes
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.features.input.model.NpadStyleIndex
import org.yuzu.yuzu_emu.features.settings.model.AbstractBooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.ByteSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.LongSetting
import org.yuzu.yuzu_emu.features.settings.model.ShortSetting
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.network.NetDataValidators
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a [AbstractSetting] object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
abstract class SettingsItem(
    val setting: AbstractSetting,
    @StringRes val titleId: Int,
    val titleString: String,
    @StringRes val descriptionId: Int,
    val descriptionString: String
) {
    abstract val type: Int

    val title: String by lazy {
        if (titleId != 0) {
            return@lazy YuzuApplication.appContext.getString(titleId)
        }
        return@lazy titleString
    }

    val description: String by lazy {
        if (descriptionId != 0) {
            return@lazy YuzuApplication.appContext.getString(descriptionId)
        }
        return@lazy descriptionString
    }

    val isEditable: Boolean
        get() {
            // Can't change docked mode toggle when using handheld mode
            if (setting.key == BooleanSetting.USE_DOCKED_MODE.key) {
                return NativeInput.getStyleIndex(0) != NpadStyleIndex.Handheld
            }

            // Can't edit settings that aren't saveable in per-game config even if they are switchable
            if (NativeConfig.isPerGameConfigLoaded() && !setting.isSaveable) {
                return false
            }

            if (!NativeLibrary.isRunning()) return true

            // Prevent editing settings that were modified in per-game config while editing global
            // config
            if (!NativeConfig.isPerGameConfigLoaded() && !setting.global) {
                return false
            }

            return setting.isRuntimeModifiable
        }

    val needsRuntimeGlobal: Boolean
        get() = NativeLibrary.isRunning() && !setting.global &&
            !NativeConfig.isPerGameConfigLoaded()

    val clearable: Boolean
        get() = !setting.global && NativeConfig.isPerGameConfigLoaded()

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_STRING_SINGLE_CHOICE = 5
        const val TYPE_DATETIME_SETTING = 6
        const val TYPE_RUNNABLE = 7
        const val TYPE_INPUT = 8
        const val TYPE_INT_SINGLE_CHOICE = 9
        const val TYPE_INPUT_PROFILE = 10
        const val TYPE_STRING_INPUT = 11
        const val TYPE_SPINBOX = 12

        const val FASTMEM_COMBINED = "fastmem_combined"

        val emptySetting = object : AbstractSetting {
            override val key: String = ""
            override val defaultValue: Any = false
            override val isSaveable = true
            override fun getValueAsString(needsGlobal: Boolean): String = ""
            override fun reset() {}
        }

        // Extension for putting SettingsItems into a hashmap without repeating yourself
        fun HashMap<String, SettingsItem>.put(item: SettingsItem) {
            put(item.setting.key, item)
        }

        // List of all general
        val settingsItems = HashMap<String, SettingsItem>().apply {
            put(StringInputSetting(StringSetting.DEVICE_NAME, titleId = R.string.device_name))
            put(
                SwitchSetting(
                    BooleanSetting.USE_LRU_CACHE,
                    titleId = R.string.use_lru_cache,
                    descriptionId = R.string.use_lru_cache_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_SPEED_LIMIT,
                    titleId = R.string.frame_limit_enable,
                    descriptionId = R.string.frame_limit_enable_description
                )
            )
            put(
                SingleChoiceSetting(
                    ByteSetting.RENDERER_DYNA_STATE,
                    titleId = R.string.dyna_state,
                    descriptionId = R.string.dyna_state_description,
                    choicesId = R.array.dynaStateEntries,
                    valuesId = R.array.dynaStateValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_PROVOKING_VERTEX,
                    titleId = R.string.provoking_vertex,
                    descriptionId = R.string.provoking_vertex_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_VERTEX_INPUT_DYNAMIC_STATE,
                    titleId = R.string.vertex_input_dynamic_state,
                    descriptionId = R.string.vertex_input_dynamic_state_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_DESCRIPTOR_INDEXING,
                    titleId = R.string.descriptor_indexing,
                    descriptionId = R.string.descriptor_indexing_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_SAMPLE_SHADING,
                    titleId = R.string.sample_shading,
                    descriptionId = R.string.sample_shading_description
                )
            )
            put(
                SliderSetting(
                    IntSetting.RENDERER_SAMPLE_SHADING_FRACTION,
                    titleId = R.string.sample_shading_fraction,
                    descriptionId = R.string.sample_shading_fraction_description,
                    units = "%"
                )
            )
            put(
                SliderSetting(
                    ShortSetting.RENDERER_SPEED_LIMIT,
                    titleId = R.string.frame_limit_slider,
                    descriptionId = R.string.frame_limit_slider_description,
                    min = 1,
                    max = 400,
                    units = "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.CPU_BACKEND,
                    titleId = R.string.cpu_backend,
                    choicesId = R.array.cpuBackendArm64Names,
                    valuesId = R.array.cpuBackendArm64Values
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.CPU_ACCURACY,
                    titleId = R.string.cpu_accuracy,
                    choicesId = R.array.cpuAccuracyNames,
                    valuesId = R.array.cpuAccuracyValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.PICTURE_IN_PICTURE,
                    titleId = R.string.picture_in_picture,
                    descriptionId = R.string.picture_in_picture_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.DEBUG_FLUSH_BY_LINE,
                    titleId = R.string.flush_by_line,
                    descriptionId = R.string.flush_by_line_description
                )
            )

            val dockedModeSetting = object : AbstractBooleanSetting {
                override val key = BooleanSetting.USE_DOCKED_MODE.key

                override fun getBoolean(needsGlobal: Boolean): Boolean {
                    if (NativeInput.getStyleIndex(0) == NpadStyleIndex.Handheld) {
                        return false
                    }
                    return BooleanSetting.USE_DOCKED_MODE.getBoolean(needsGlobal)
                }

                override fun setBoolean(value: Boolean) =
                    BooleanSetting.USE_DOCKED_MODE.setBoolean(value)

                override val defaultValue = BooleanSetting.USE_DOCKED_MODE.defaultValue

                override fun getValueAsString(needsGlobal: Boolean): String =
                    BooleanSetting.USE_DOCKED_MODE.getValueAsString(needsGlobal)

                override fun reset() = BooleanSetting.USE_DOCKED_MODE.reset()
            }
            put(
                SwitchSetting(
                    BooleanSetting.FRAME_INTERPOLATION,
                    titleId = R.string.frame_interpolation,
                    descriptionId = R.string.frame_interpolation_description
                )
            )

//            put(
//                SwitchSetting(
//                    BooleanSetting.FRAME_SKIPPING,
//                    titleId = R.string.frame_skipping,
//                    descriptionId = R.string.frame_skipping_description
//                )
//            )

            put(
                SwitchSetting(
                    dockedModeSetting,
                    titleId = R.string.use_docked_mode,
                    descriptionId = R.string.use_docked_mode_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.MEMORY_LAYOUT,
                    titleId = R.string.memory_layout,
                    descriptionId = R.string.memory_layout_description,
                    choicesId = R.array.memoryNames,
                    valuesId = R.array.memoryValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.CORE_SYNC_CORE_SPEED,
                    titleId = R.string.use_sync_core,
                    descriptionId = R.string.use_sync_core_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.REGION_INDEX,
                    titleId = R.string.emulated_region,
                    choicesId = R.array.regionNames,
                    valuesId = R.array.regionValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.LANGUAGE_INDEX,
                    titleId = R.string.emulated_language,
                    choicesId = R.array.languageNames,
                    valuesId = R.array.languageValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_CUSTOM_RTC,
                    titleId = R.string.use_custom_rtc,
                    descriptionId = R.string.use_custom_rtc_description
                )
            )
            put(
                StringInputSetting(
                    StringSetting.WEB_TOKEN,
                    titleId = R.string.web_token,
                    descriptionId = R.string.web_token_description,
                    onGenerate = {
                        val chars = "abcdefghijklmnopqrstuvwxyz"
                        (1..48).map { chars.random() }.joinToString("")
                    },
                    validator = NetDataValidators::token,
                    errorId = R.string.multiplayer_token_error
                )
            )

            put(
                StringInputSetting(
                    StringSetting.WEB_USERNAME,
                    titleId = R.string.web_username,
                    descriptionId = R.string.web_username_description,
                    validator = NetDataValidators::username,
                    errorId = R.string.multiplayer_username_error
                )
            )
            put(DateTimeSetting(LongSetting.CUSTOM_RTC, titleId = R.string.set_custom_rtc))
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ACCURACY,
                    titleId = R.string.renderer_accuracy,
                    choicesId = R.array.rendererAccuracyNames,
                    valuesId = R.array.rendererAccuracyValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SHADER_BACKEND,
                    titleId = R.string.shader_backend,
                    descriptionId = R.string.shader_backend_description,
                    choicesId = R.array.rendererShaderNames,
                    valuesId = R.array.rendererShaderValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_NVDEC_EMULATION,
                    titleId = R.string.nvdec_emulation,
                    descriptionId = R.string.nvdec_emulation_description,
                    choicesId = R.array.rendererNvdecNames,
                    valuesId = R.array.rendererNvdecValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASTC_DECODE_METHOD,
                    titleId = R.string.accelerate_astc,
                    descriptionId = R.string.accelerate_astc_description,
                    choicesId = R.array.astcDecodingMethodNames,
                    valuesId = R.array.astcDecodingMethodValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASTC_RECOMPRESSION,
                    titleId = R.string.astc_recompression,
                    descriptionId = R.string.astc_recompression_description,
                    choicesId = R.array.astcRecompressionMethodNames,
                    valuesId = R.array.astcRecompressionMethodValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_VRAM_USAGE_MODE,
                    titleId = R.string.vram_usage_mode,
                    descriptionId = R.string.vram_usage_mode_description,
                    choicesId = R.array.vramUsageMethodNames,
                    valuesId = R.array.vramUsageMethodValues
                )
            )

            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_RESOLUTION,
                    titleId = R.string.renderer_resolution,
                    choicesId = R.array.rendererResolutionNames,
                    valuesId = R.array.rendererResolutionValues,
                    warnChoices = (5..7).toList(),
                    warningMessage = R.string.warning_resolution
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.ENABLE_INPUT_OVERLAY_AUTO_HIDE,
                    titleId = R.string.enable_input_overlay_auto_hide,
                )
            )
            put(
                SpinBoxSetting(
                    IntSetting.INPUT_OVERLAY_AUTO_HIDE,
                    titleId = R.string.overlay_auto_hide,
                    descriptionId = R.string.overlay_auto_hide_description,
                    min = 1,
                    max = 999,
                    valueHint = R.string.seconds
                )
            )

            put(
                SwitchSetting(
                    BooleanSetting.SHOW_PERFORMANCE_OVERLAY,
                    R.string.enable_stats_overlay_,
                    descriptionId = R.string.stats_overlay_options_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.PERF_OVERLAY_BACKGROUND,
                    R.string.perf_overlay_background,
                    descriptionId = R.string.perf_overlay_background_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.PERF_OVERLAY_POSITION,
                    titleId = R.string.overlay_position,
                    descriptionId = R.string.overlay_position_description,
                    choicesId = R.array.statsPosition,
                    valuesId = R.array.staticThemeValues
                )
            )

            put(
                SwitchSetting(
                    BooleanSetting.SHOW_FPS,
                    R.string.show_fps,
                    descriptionId = R.string.show_fps_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_FRAMETIME,
                    R.string.show_frametime,
                    descriptionId = R.string.show_frametime_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_APP_RAM_USAGE,
                    R.string.show_app_ram_usage,
                    descriptionId = R.string.show_app_ram_usage_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_SYSTEM_RAM_USAGE,
                    R.string.show_system_ram_usage,
                    descriptionId = R.string.show_system_ram_usage_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_BAT_TEMPERATURE,
                    R.string.show_bat_temperature,
                    descriptionId = R.string.show_bat_temperature_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.BAT_TEMPERATURE_UNIT,
                    R.string.bat_temperature_unit,
                    choicesId = R.array.temperatureUnitEntries,
                    valuesId = R.array.temperatureUnitValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_POWER_INFO,
                    R.string.show_power_info,
                    descriptionId = R.string.show_power_info_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_SHADERS_BUILDING,
                    R.string.show_shaders_building,
                    descriptionId = R.string.show_shaders_building_description
                )
            )

            put(
                SwitchSetting(
                    BooleanSetting.SHOW_SOC_OVERLAY,
                    R.string.enable_soc_overlay,
                    descriptionId = R.string.soc_overlay_options_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SOC_OVERLAY_BACKGROUND,
                    R.string.perf_overlay_background,
                    descriptionId = R.string.perf_overlay_background_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.SOC_OVERLAY_POSITION,
                    titleId = R.string.overlay_position,
                    descriptionId = R.string.overlay_position_description,
                    choicesId = R.array.statsPosition,
                    valuesId = R.array.staticThemeValues
                )
            )

            put(
                SwitchSetting(
                    BooleanSetting.SHOW_DEVICE_MODEL,
                    titleId = R.string.show_device_model,
                    descriptionId = R.string.show_device_model_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_GPU_MODEL,
                    titleId = R.string.show_gpu_model,
                    descriptionId = R.string.show_gpu_model_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_SOC_MODEL,
                    titleId = R.string.show_soc_model,
                    descriptionId = R.string.show_soc_model_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SHOW_FW_VERSION,
                    titleId = R.string.show_fw_version,
                    descriptionId = R.string.show_fw_version_description
                )
            )

            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_VSYNC,
                    titleId = R.string.renderer_vsync,
                    choicesId = R.array.rendererVSyncNames,
                    valuesId = R.array.rendererVSyncValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCALING_FILTER,
                    titleId = R.string.renderer_scaling_filter,
                    choicesId = R.array.rendererScalingFilterNames,
                    valuesId = R.array.rendererScalingFilterValues
                )
            )
            put(
                SliderSetting(
                    IntSetting.FSR_SHARPENING_SLIDER,
                    titleId = R.string.fsr_sharpness,
                    descriptionId = R.string.fsr_sharpness_description,
                    units = "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ANTI_ALIASING,
                    titleId = R.string.renderer_anti_aliasing,
                    choicesId = R.array.rendererAntiAliasingNames,
                    valuesId = R.array.rendererAntiAliasingValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_SCREEN_LAYOUT,
                    titleId = R.string.renderer_screen_layout,
                    choicesId = R.array.rendererScreenLayoutNames,
                    valuesId = R.array.rendererScreenLayoutValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_ASPECT_RATIO,
                    titleId = R.string.renderer_aspect_ratio,
                    choicesId = R.array.rendererAspectRatioNames,
                    valuesId = R.array.rendererAspectRatioValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.VERTICAL_ALIGNMENT,
                    titleId = R.string.vertical_alignment,
                    descriptionId = 0,
                    choicesId = R.array.verticalAlignmentEntries,
                    valuesId = R.array.verticalAlignmentValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_USE_DISK_SHADER_CACHE,
                    titleId = R.string.use_disk_shader_cache,
                    descriptionId = R.string.use_disk_shader_cache_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_FORCE_MAX_CLOCK,
                    titleId = R.string.renderer_force_max_clock,
                    descriptionId = R.string.renderer_force_max_clock_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_OPTIMIZE_SPIRV_OUTPUT,
                    titleId = R.string.renderer_optimize_spirv_output,
                    descriptionId = R.string.renderer_optimize_spirv_output_description,
                    choicesId = R.array.optimizeSpirvOutputEntries,
                    valuesId = R.array.optimizeSpirvOutputValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.DMA_ACCURACY,
                    titleId = R.string.dma_accuracy,
                    descriptionId = R.string.dma_accuracy_description,
                    choicesId = R.array.dmaAccuracyNames,
                    valuesId = R.array.dmaAccuracyValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_ASYNCHRONOUS_SHADERS,
                    titleId = R.string.renderer_asynchronous_shaders,
                    descriptionId = R.string.renderer_asynchronous_shaders_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_FAST_GPU,
                    titleId = R.string.use_fast_gpu_time,
                    descriptionId = R.string.use_fast_gpu_time_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.FAST_GPU_TIME,
                    titleId = R.string.fast_gpu_time,
                    descriptionId = R.string.fast_gpu_time_description,
                    choicesId = R.array.gpuEntries,
                    valuesId = R.array.gpuValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_FAST_CPU_TIME,
                    titleId = R.string.use_fast_cpu_time,
                    descriptionId = R.string.use_fast_cpu_time_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.FAST_CPU_TIME,
                    titleId = R.string.fast_cpu_time,
                    descriptionId = R.string.fast_cpu_time_description,
                    choicesId = R.array.clockNames,
                    valuesId = R.array.clockValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_CUSTOM_CPU_TICKS,
                    titleId = R.string.custom_cpu_ticks,
                    descriptionId = R.string.custom_cpu_ticks_description
                )
            )
            put(
                SpinBoxSetting(
                    IntSetting.CPU_TICKS,
                    titleId = R.string.cpu_ticks,
                    descriptionId = 0,
                    valueHint = R.string.cpu_ticks,
                    min = 77,
                    max = 65535
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SKIP_CPU_INNER_INVALIDATION,
                    titleId = R.string.skip_cpu_inner_invalidation,
                    descriptionId = R.string.skip_cpu_inner_invalidation_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.CPUOPT_UNSAFE_HOST_MMU,
                    titleId = R.string.cpuopt_unsafe_host_mmu,
                    descriptionId = R.string.cpuopt_unsafe_host_mmu_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_REACTIVE_FLUSHING,
                    titleId = R.string.renderer_reactive_flushing,
                    descriptionId = R.string.renderer_reactive_flushing_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_EARLY_RELEASE_FENCES,
                    titleId = R.string.renderer_early_release_fences,
                    descriptionId = R.string.renderer_early_release_fences_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.SYNC_MEMORY_OPERATIONS,
                    titleId = R.string.sync_memory_operations,
                    descriptionId = R.string.sync_memory_operations_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.BUFFER_REORDER_DISABLE,
                    titleId = R.string.buffer_reorder_disable,
                    descriptionId = R.string.buffer_reorder_disable_description
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.MAX_ANISOTROPY,
                    titleId = R.string.anisotropic_filtering,
                    descriptionId = R.string.anisotropic_filtering_description,
                    choicesId = R.array.anisoEntries,
                    valuesId = R.array.anisoValues
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.AUDIO_OUTPUT_ENGINE,
                    titleId = R.string.audio_output_engine,
                    choicesId = R.array.outputEngineEntries,
                    valuesId = R.array.outputEngineValues
                )
            )
            put(
                SliderSetting(
                    ByteSetting.AUDIO_VOLUME,
                    titleId = R.string.audio_volume,
                    descriptionId = R.string.audio_volume_description,
                    units = "%"
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.RENDERER_BACKEND,
                    titleId = R.string.renderer_api,
                    choicesId = R.array.rendererApiNames,
                    valuesId = R.array.rendererApiValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.ENABLE_UPDATE_CHECKS,
                    titleId = R.string.enable_update_checks,
                    descriptionId = R.string.enable_update_checks_description,
                )
            )
            put(
                SingleChoiceSetting(
                    IntSetting.APP_LANGUAGE,
                    titleId = R.string.app_language,
                    descriptionId = R.string.app_language_description,
                    choicesId = R.array.appLanguageNames,
                    valuesId = R.array.appLanguageValues
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.RENDERER_DEBUG,
                    titleId = R.string.renderer_debug,
                    descriptionId = R.string.renderer_debug_description
                )
            )
            put(
                SwitchSetting(
                    BooleanSetting.USE_AUTO_STUB,
                    titleId = R.string.use_auto_stub,
                    descriptionId = R.string.use_auto_stub_description
                )
            )

            val fastmem = object : AbstractBooleanSetting {
                override fun getBoolean(needsGlobal: Boolean): Boolean =
                    BooleanSetting.FASTMEM.getBoolean() &&
                        BooleanSetting.FASTMEM_EXCLUSIVES.getBoolean()

                override fun setBoolean(value: Boolean) {
                    BooleanSetting.FASTMEM.setBoolean(value)
                    BooleanSetting.FASTMEM_EXCLUSIVES.setBoolean(value)
                }

                override val key: String = FASTMEM_COMBINED
                override val isRuntimeModifiable: Boolean = false
                override val defaultValue: Boolean = true
                override val isSwitchable: Boolean = true
                override var global: Boolean
                    get() {
                        return BooleanSetting.FASTMEM.global &&
                            BooleanSetting.FASTMEM_EXCLUSIVES.global
                    }
                    set(value) {
                        BooleanSetting.FASTMEM.global = value
                        BooleanSetting.FASTMEM_EXCLUSIVES.global = value
                    }

                override val isSaveable = true

                override fun getValueAsString(needsGlobal: Boolean): String =
                    getBoolean().toString()

                override fun reset() = setBoolean(defaultValue)
            }
            put(SwitchSetting(fastmem, R.string.fastmem))

            // Applet Settings
            put(
                SingleChoiceSetting(
                    IntSetting.SWKBD_APPLET,
                    titleId = R.string.swkbd_applet,
                    choicesId = R.array.appletEntries,
                    valuesId = R.array.appletValues
                )
            )

            put(
                SwitchSetting(
                    BooleanSetting.AIRPLANE_MODE,
                    titleId = R.string.airplane_mode,
                    descriptionId = R.string.airplane_mode_description
                )
            )
        }
    }
}

