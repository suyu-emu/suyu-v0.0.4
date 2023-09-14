// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

enum class ByteSetting(
    override val key: String,
    override val category: Settings.Category
) : AbstractByteSetting {
    AUDIO_VOLUME("volume", Settings.Category.Audio);

    override val byte: Byte
        get() = NativeConfig.getByte(key, false)

    override fun setByte(value: Byte) = NativeConfig.setByte(key, value)

    override val defaultValue: Byte by lazy { NativeConfig.getByte(key, true) }

    override val valueAsString: String
        get() = byte.toString()

    override fun reset() = NativeConfig.setByte(key, defaultValue)
}
