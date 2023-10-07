// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class StringSetting(
    override val key: String,
    override val category: Settings.Category
) : AbstractStringSetting {
    // No string settings currently exist
    EMPTY_SETTING("", Settings.Category.UiGeneral);

    override val string: String
        get() = NativeConfig.getString(key, false)

    override fun setString(value: String) = NativeConfig.setString(key, value)

    override val defaultValue: String by lazy { NativeConfig.getString(key, true) }

    override val valueAsString: String
        get() = string

    override fun reset() = NativeConfig.setString(key, defaultValue)
}
