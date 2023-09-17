// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class ShortSetting(
    override val key: String,
    override val category: Settings.Category
) : AbstractShortSetting {
    RENDERER_SPEED_LIMIT("speed_limit", Settings.Category.Core);

    override val short: Short
        get() = NativeConfig.getShort(key, false)

    override fun setShort(value: Short) = NativeConfig.setShort(key, value)

    override val defaultValue: Short by lazy { NativeConfig.getShort(key, true) }

    override val valueAsString: String
        get() = short.toString()

    override fun reset() = NativeConfig.setShort(key, defaultValue)
}
