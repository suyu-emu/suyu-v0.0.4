// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.features.settings.model

import org.suyu.suyu_emu.utils.NativeConfig

enum class ByteSetting(override val key: String) : AbstractByteSetting {
    AUDIO_VOLUME("volume");

    override fun getByte(needsGlobal: Boolean): Byte = NativeConfig.getByte(key, needsGlobal)

    override fun setByte(value: Byte) {
        if (NativeConfig.isPerGameConfigLoaded()) {
            global = false
        }
        NativeConfig.setByte(key, value)
    }

    override val defaultValue: Byte by lazy { NativeConfig.getDefaultToString(key).toByte() }

    override fun getValueAsString(needsGlobal: Boolean): String = getByte(needsGlobal).toString()

    override fun reset() = NativeConfig.setByte(key, defaultValue)
}
