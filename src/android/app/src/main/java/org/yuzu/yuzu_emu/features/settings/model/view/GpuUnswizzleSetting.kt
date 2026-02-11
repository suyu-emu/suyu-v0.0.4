// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.ArrayRes
import androidx.annotation.StringRes
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting

class GpuUnswizzleSetting(
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    @ArrayRes val textureSizeChoicesId: Int,
    @ArrayRes val textureSizeValuesId: Int,
    @ArrayRes val streamSizeChoicesId: Int,
    @ArrayRes val streamSizeValuesId: Int,
    @ArrayRes val chunkSizeChoicesId: Int,
    @ArrayRes val chunkSizeValuesId: Int
) : SettingsItem(
    object : AbstractSetting {
        override val key: String = SettingsItem.GPU_UNSWIZZLE_COMBINED
        override val defaultValue: Any = false
        override val isSaveable = true
        override val isRuntimeModifiable = true
        override val isSwitchable = true
        override fun getValueAsString(needsGlobal: Boolean): String = "combined"
        override fun reset() {
            BooleanSetting.GPU_UNSWIZZLE_ENABLED.reset()
            IntSetting.GPU_UNSWIZZLE_TEXTURE_SIZE.reset()
            IntSetting.GPU_UNSWIZZLE_STREAM_SIZE.reset()
            IntSetting.GPU_UNSWIZZLE_CHUNK_SIZE.reset()
        }
    },
    titleId,
    titleString,
    descriptionId,
    descriptionString
) {
    override val type = SettingsItem.TYPE_GPU_UNSWIZZLE

    // Check if GPU unswizzle is enabled via the dedicated boolean setting
    fun isEnabled(needsGlobal: Boolean = false): Boolean =
        BooleanSetting.GPU_UNSWIZZLE_ENABLED.getBoolean(needsGlobal)

    fun setEnabled(value: Boolean) =
        BooleanSetting.GPU_UNSWIZZLE_ENABLED.setBoolean(value)

    fun enable() = setEnabled(true)

    fun disable() = setEnabled(false)

    fun getTextureSize(needsGlobal: Boolean = false): Int =
        IntSetting.GPU_UNSWIZZLE_TEXTURE_SIZE.getInt(needsGlobal)

    fun setTextureSize(value: Int) =
        IntSetting.GPU_UNSWIZZLE_TEXTURE_SIZE.setInt(value)

    fun getStreamSize(needsGlobal: Boolean = false): Int =
        IntSetting.GPU_UNSWIZZLE_STREAM_SIZE.getInt(needsGlobal)

    fun setStreamSize(value: Int) =
        IntSetting.GPU_UNSWIZZLE_STREAM_SIZE.setInt(value)

    fun getChunkSize(needsGlobal: Boolean = false): Int =
        IntSetting.GPU_UNSWIZZLE_CHUNK_SIZE.getInt(needsGlobal)

    fun setChunkSize(value: Int) =
        IntSetting.GPU_UNSWIZZLE_CHUNK_SIZE.setInt(value)

    fun reset() = setting.reset()
}