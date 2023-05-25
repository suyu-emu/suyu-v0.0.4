// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

enum class IntSetting(
    override val key: String,
    override val section: String,
    override val defaultValue: Int
) : AbstractIntSetting {
    RENDERER_USE_SPEED_LIMIT(
        "use_speed_limit",
        Settings.SECTION_RENDERER,
        1
    ),
    USE_DOCKED_MODE(
        "use_docked_mode",
        Settings.SECTION_SYSTEM,
        0
    ),
    RENDERER_USE_DISK_SHADER_CACHE(
        "use_disk_shader_cache",
        Settings.SECTION_RENDERER,
        1
    ),
    RENDERER_FORCE_MAX_CLOCK(
        "force_max_clock",
        Settings.SECTION_RENDERER,
        1
    ),
    RENDERER_ASYNCHRONOUS_SHADERS(
        "use_asynchronous_shaders",
        Settings.SECTION_RENDERER,
        0
    ),
    RENDERER_DEBUG(
        "debug",
        Settings.SECTION_RENDERER,
        0
    ),
    RENDERER_SPEED_LIMIT(
        "speed_limit",
        Settings.SECTION_RENDERER,
        100
    ),
    CPU_ACCURACY(
        "cpu_accuracy",
        Settings.SECTION_CPU,
        0
    ),
    REGION_INDEX(
        "region_index",
        Settings.SECTION_SYSTEM,
        -1
    ),
    LANGUAGE_INDEX(
        "language_index",
        Settings.SECTION_SYSTEM,
        1
    ),
    RENDERER_BACKEND(
        "backend",
        Settings.SECTION_RENDERER,
        1
    ),
    RENDERER_ACCURACY(
        "gpu_accuracy",
        Settings.SECTION_RENDERER,
        0
    ),
    RENDERER_RESOLUTION(
        "resolution_setup",
        Settings.SECTION_RENDERER,
        2
    ),
    RENDERER_VSYNC(
        "use_vsync",
        Settings.SECTION_RENDERER,
        0
    ),
    RENDERER_SCALING_FILTER(
        "scaling_filter",
        Settings.SECTION_RENDERER,
        1
    ),
    RENDERER_ANTI_ALIASING(
        "anti_aliasing",
        Settings.SECTION_RENDERER,
        0
    ),
    RENDERER_ASPECT_RATIO(
        "aspect_ratio",
        Settings.SECTION_RENDERER,
        0
    ),
    AUDIO_VOLUME(
        "volume",
        Settings.SECTION_AUDIO,
        100
    );

    override var int: Int = defaultValue

    override val valueAsString: String
        get() = int.toString()

    override val isRuntimeEditable: Boolean
        get() {
            for (setting in NOT_RUNTIME_EDITABLE) {
                if (setting == this) {
                    return false
                }
            }
            return true
        }

    companion object {
        private val NOT_RUNTIME_EDITABLE = listOf(
            RENDERER_USE_DISK_SHADER_CACHE,
            RENDERER_ASYNCHRONOUS_SHADERS,
            RENDERER_DEBUG,
            RENDERER_BACKEND,
            RENDERER_RESOLUTION,
            RENDERER_VSYNC
        )

        fun from(key: String): IntSetting? = IntSetting.values().firstOrNull { it.key == key }

        fun clear() = IntSetting.values().forEach { it.int = it.defaultValue }
    }
}
