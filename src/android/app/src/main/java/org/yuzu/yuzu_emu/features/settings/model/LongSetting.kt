// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class LongSetting(
    override val key: String,
    override val category: Settings.Category
) : AbstractLongSetting {
    CUSTOM_RTC("custom_rtc", Settings.Category.System);

    override val long: Long
        get() = NativeConfig.getLong(key, false)

    override fun setLong(value: Long) = NativeConfig.setLong(key, value)

    override val defaultValue: Long by lazy { NativeConfig.getLong(key, true) }

    override val valueAsString: String
        get() = long.toString()

    override fun reset() = NativeConfig.setLong(key, defaultValue)
}
