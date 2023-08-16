// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class FloatSetting(
    override val key: String,
    override val category: Settings.Category
) : AbstractFloatSetting {
    // No float settings currently exist
    EMPTY_SETTING("", Settings.Category.UiGeneral);

    override val float: Float
        get() = NativeConfig.getFloat(key, false)

    override fun setFloat(value: Float) = NativeConfig.setFloat(key, value)

    override val defaultValue: Float by lazy { NativeConfig.getFloat(key, true) }

    override val valueAsString: String
        get() = float.toString()

    override fun reset() = NativeConfig.setFloat(key, defaultValue)
}
