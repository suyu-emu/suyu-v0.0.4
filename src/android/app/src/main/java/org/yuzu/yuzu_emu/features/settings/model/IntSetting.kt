// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class IntSetting(
    override val key: String,
    override val category: Settings.Category,
    override val androidDefault: Int? = null
) : AbstractIntSetting {
    CPU_BACKEND("cpu_backend", Settings.Category.Cpu),
    CPU_ACCURACY("cpu_accuracy", Settings.Category.Cpu),
    REGION_INDEX("region_index", Settings.Category.System),
    LANGUAGE_INDEX("language_index", Settings.Category.System),
    RENDERER_BACKEND("backend", Settings.Category.Renderer),
    RENDERER_ACCURACY("gpu_accuracy", Settings.Category.Renderer, 0),
    RENDERER_RESOLUTION("resolution_setup", Settings.Category.Renderer),
    RENDERER_VSYNC("use_vsync", Settings.Category.Renderer),
    RENDERER_SCALING_FILTER("scaling_filter", Settings.Category.Renderer),
    RENDERER_ANTI_ALIASING("anti_aliasing", Settings.Category.Renderer),
    RENDERER_SCREEN_LAYOUT("screen_layout", Settings.Category.Android),
    RENDERER_ASPECT_RATIO("aspect_ratio", Settings.Category.Renderer),
    AUDIO_OUTPUT_ENGINE("output_engine", Settings.Category.Audio);

    override val int: Int
        get() = NativeConfig.getInt(key, false)

    override fun setInt(value: Int) = NativeConfig.setInt(key, value)

    override val defaultValue: Int by lazy {
        androidDefault ?: NativeConfig.getInt(key, true)
    }

    override val valueAsString: String
        get() = int.toString()

    override fun reset() = NativeConfig.setInt(key, defaultValue)
}
