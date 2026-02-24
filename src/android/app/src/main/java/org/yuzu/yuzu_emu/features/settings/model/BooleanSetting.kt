// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class BooleanSetting(override val key: String) : AbstractBooleanSetting {
    AUDIO_MUTED("audio_muted"),
    FASTMEM("cpuopt_fastmem"),
    FASTMEM_EXCLUSIVES("cpuopt_fastmem_exclusives"),
    CORE_SYNC_CORE_SPEED("sync_core_speed"),
    RENDERER_USE_SPEED_LIMIT("use_speed_limit"),
    USE_CUSTOM_CPU_TICKS("use_custom_cpu_ticks"),
    SKIP_CPU_INNER_INVALIDATION("skip_cpu_inner_invalidation"),
    FIX_BLOOM_EFFECTS("fix_bloom_effects"),
    CPUOPT_UNSAFE_HOST_MMU("cpuopt_unsafe_host_mmu"),
    USE_DOCKED_MODE("use_docked_mode"),
    USE_AUTO_STUB("use_auto_stub"),
    RENDERER_USE_DISK_SHADER_CACHE("use_disk_shader_cache"),
    RENDERER_FORCE_MAX_CLOCK("force_max_clock"),
    RENDERER_ASYNCHRONOUS_SHADERS("use_asynchronous_shaders"),
    RENDERER_REACTIVE_FLUSHING("use_reactive_flushing"),
    ENABLE_BUFFER_HISTORY("enable_buffer_history"),
    USE_OPTIMIZED_VERTEX_BUFFERS("use_optimized_vertex_buffers"),
    SYNC_MEMORY_OPERATIONS("sync_memory_operations"),
    BUFFER_REORDER_DISABLE("disable_buffer_reorder"),
    RENDERER_DEBUG("debug"),
    RENDERER_PATCH_OLD_QCOM_DRIVERS("patch_old_qcom_drivers"),
    RENDERER_VERTEX_INPUT_DYNAMIC_STATE("vertex_input_dynamic_state"),
    RENDERER_PROVOKING_VERTEX("provoking_vertex"),
    RENDERER_DESCRIPTOR_INDEXING("descriptor_indexing"),
    RENDERER_SAMPLE_SHADING("sample_shading"),
    GPU_UNSWIZZLE_ENABLED("gpu_unswizzle_enabled"),
    PICTURE_IN_PICTURE("picture_in_picture"),
    USE_CUSTOM_RTC("custom_rtc_enabled"),
    BLACK_BACKGROUNDS("black_backgrounds"),
    INVERT_CONFIRM_BACK_CONTROLLER_BUTTONS("invert_confirm_back_controller_buttons"),

    ENABLE_FOLDER_BUTTON("enable_folder_button"),
    ENABLE_QLAUNCH_BUTTON("enable_qlaunch_button"),

    ENABLE_UPDATE_CHECKS("enable_update_checks"),
    JOYSTICK_REL_CENTER("joystick_rel_center"),
    DPAD_SLIDE("dpad_slide"),
    HAPTIC_FEEDBACK("haptic_feedback"),
    SHOW_INPUT_OVERLAY("show_input_overlay"),
    OVERLAY_SNAP_TO_GRID("overlay_snap_to_grid"),
    TOUCHSCREEN("touchscreen"),
    AIRPLANE_MODE("airplane_mode"),

    SHOW_SOC_OVERLAY("show_soc_overlay"),
    SHOW_BUILD_ID("show_build_id"),
    SHOW_DRIVER_VERSION("show_driver_version"),
    SHOW_DEVICE_MODEL("show_device_model"),
    SHOW_GPU_MODEL("show_gpu_model"),
    SHOW_SOC_MODEL("show_soc_model"),
    SHOW_FW_VERSION("show_firmware_version"),

    SOC_OVERLAY_BACKGROUND("soc_overlay_background"),
    ENABLE_INPUT_OVERLAY_AUTO_HIDE("enable_input_overlay_auto_hide"),
    HIDE_OVERLAY_ON_CONTROLLER_INPUT("hide_overlay_on_controller_input"),

    PERF_OVERLAY_BACKGROUND("perf_overlay_background"),
    SHOW_PERFORMANCE_OVERLAY("show_performance_overlay"),

    SHOW_FPS("show_fps"),
    SHOW_FRAMETIME("show_frame_time"),
    SHOW_APP_RAM_USAGE("show_app_ram_usage"),
    SHOW_SYSTEM_RAM_USAGE("show_system_ram_usage"),
    SHOW_BAT_TEMPERATURE("show_bat_temperature"),
    SHOW_POWER_INFO("show_power_info"),
    SHOW_SHADERS_BUILDING("show_shaders_building"),

    DEBUG_FLUSH_BY_LINE("flush_line"),
    DONT_SHOW_DRIVER_SHADER_WARNING("dont_show_driver_shader_warning"),
    ENABLE_OVERLAY("enable_overlay"),

    // GPU Logging
    GPU_LOGGING_ENABLED("gpu_logging_enabled"),
    GPU_LOG_VULKAN_CALLS("gpu_log_vulkan_calls"),
    GPU_LOG_SHADER_DUMPS("gpu_log_shader_dumps"),
    GPU_LOG_MEMORY_TRACKING("gpu_log_memory_tracking"),
    GPU_LOG_DRIVER_DEBUG("gpu_log_driver_debug"),

    ENABLE_QUICK_SETTINGS("enable_quick_settings");

//  external fun isFrameSkippingEnabled(): Boolean
    external fun isFrameInterpolationEnabled(): Boolean

    override fun getBoolean(needsGlobal: Boolean): Boolean =
        NativeConfig.getBoolean(key, needsGlobal)

    override fun setBoolean(value: Boolean) {
        if (NativeConfig.isPerGameConfigLoaded()) {
            global = false
        }
        NativeConfig.setBoolean(key, value)
    }

    override val defaultValue: Boolean by lazy { NativeConfig.getDefaultToString(key).toBoolean() }

    override fun getValueAsString(needsGlobal: Boolean): String = getBoolean(needsGlobal).toString()

    override fun reset() = NativeConfig.setBoolean(key, defaultValue)
}
