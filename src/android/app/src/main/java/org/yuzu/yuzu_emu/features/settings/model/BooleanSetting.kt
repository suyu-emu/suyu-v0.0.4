// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class BooleanSetting(
    override val key: String,
    override val category: Settings.Category
) : AbstractBooleanSetting {
    CPU_DEBUG_MODE("cpu_debug_mode", Settings.Category.Cpu),
    FASTMEM("cpuopt_fastmem", Settings.Category.Cpu),
    FASTMEM_EXCLUSIVES("cpuopt_fastmem_exclusives", Settings.Category.Cpu),
    RENDERER_USE_SPEED_LIMIT("use_speed_limit", Settings.Category.Core),
    USE_DOCKED_MODE("use_docked_mode", Settings.Category.System),
    RENDERER_USE_DISK_SHADER_CACHE("use_disk_shader_cache", Settings.Category.Renderer),
    RENDERER_FORCE_MAX_CLOCK("force_max_clock", Settings.Category.Renderer),
    RENDERER_ASYNCHRONOUS_SHADERS("use_asynchronous_shaders", Settings.Category.Renderer),
    RENDERER_REACTIVE_FLUSHING("use_reactive_flushing", Settings.Category.Renderer),
    RENDERER_DEBUG("debug", Settings.Category.Renderer),
    PICTURE_IN_PICTURE("picture_in_picture", Settings.Category.Android),
    USE_CUSTOM_RTC("custom_rtc_enabled", Settings.Category.System);

    override val boolean: Boolean
        get() = NativeConfig.getBoolean(key, false)

    override fun setBoolean(value: Boolean) = NativeConfig.setBoolean(key, value)

    override val defaultValue: Boolean by lazy { NativeConfig.getBoolean(key, true) }

    override val valueAsString: String
        get() = if (boolean) "1" else "0"

    override fun reset() = NativeConfig.setBoolean(key, defaultValue)
}
