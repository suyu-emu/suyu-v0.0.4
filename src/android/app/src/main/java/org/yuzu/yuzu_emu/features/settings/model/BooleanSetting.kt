// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class BooleanSetting(override val key: String) : AbstractBooleanSetting {
    AUDIO_MUTED("audio_muted"),
    CPU_DEBUG_MODE("cpu_debug_mode"),
    FASTMEM("cpuopt_fastmem"),
    FASTMEM_EXCLUSIVES("cpuopt_fastmem_exclusives"),
    CORE_SYNC_CORE_SPEED("sync_core_speed"),
    RENDERER_USE_SPEED_LIMIT("use_speed_limit"),
    USE_FAST_CPU_TIME("use_fast_cpu_time"),
    USE_CUSTOM_CPU_TICKS("use_custom_cpu_ticks"),
    SKIP_CPU_INNER_INVALIDATION("skip_cpu_inner_invalidation"),
    CPUOPT_UNSAFE_HOST_MMU("cpuopt_unsafe_host_mmu"),
    USE_DOCKED_MODE("use_docked_mode"),
    USE_AUTO_STUB("use_auto_stub"),
    RENDERER_USE_DISK_SHADER_CACHE("use_disk_shader_cache"),
    RENDERER_FORCE_MAX_CLOCK("force_max_clock"),
    RENDERER_ASYNCHRONOUS_SHADERS("use_asynchronous_shaders"),
    RENDERER_FAST_GPU("use_fast_gpu_time"),
    RENDERER_REACTIVE_FLUSHING("use_reactive_flushing"),
    RENDERER_EARLY_RELEASE_FENCES("early_release_fences"),
    SYNC_MEMORY_OPERATIONS("sync_memory_operations"),
    BUFFER_REORDER_DISABLE("disable_buffer_reorder"),
    RENDERER_DEBUG("debug"),
    RENDERER_PROVOKING_VERTEX("provoking_vertex"),
    RENDERER_DESCRIPTOR_INDEXING("descriptor_indexing"),
    RENDERER_SAMPLE_SHADING("sample_shading"),
    PICTURE_IN_PICTURE("picture_in_picture"),
    USE_CUSTOM_RTC("custom_rtc_enabled"),
    BLACK_BACKGROUNDS("black_backgrounds"),
    JOYSTICK_REL_CENTER("joystick_rel_center"),
    DPAD_SLIDE("dpad_slide"),
    HAPTIC_FEEDBACK("haptic_feedback"),
    SHOW_INPUT_OVERLAY("show_input_overlay"),
    TOUCHSCREEN("touchscreen"),
    AIRPLANE_MODE("airplane_mode"),

    SHOW_SOC_OVERLAY("show_soc_overlay"),
    SHOW_DEVICE_MODEL("show_device_model"),
    SHOW_GPU_MODEL("show_gpu_model"),
    SHOW_SOC_MODEL("show_soc_model"),
    SHOW_FW_VERSION("show_firmware_version"),

    SOC_OVERLAY_BACKGROUND("soc_overlay_background"),

    ENABLE_RAII("enable_raii"),
    FRAME_INTERPOLATION("frame_interpolation"),
//    FRAME_SKIPPING("frame_skipping"),

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
    USE_LRU_CACHE("use_lru_cache");

    external fun isRaiiEnabled(): Boolean

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
