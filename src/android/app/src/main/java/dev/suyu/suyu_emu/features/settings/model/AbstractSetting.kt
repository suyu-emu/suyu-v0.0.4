// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package dev.suyu.suyu_emu.features.settings.model

import dev.suyu.suyu_emu.utils.NativeConfig

interface AbstractSetting {
    val key: String
    val defaultValue: Any

    val isRuntimeModifiable: Boolean
        get() = NativeConfig.getIsRuntimeModifiable(key)

    val pairedSettingKey: String
        get() = NativeConfig.getPairedSettingKey(key)

    val isSwitchable: Boolean
        get() = NativeConfig.getIsSwitchable(key)

    var global: Boolean
        get() = NativeConfig.usingGlobal(key)
        set(value) = NativeConfig.setGlobal(key, value)

    val isSaveable: Boolean
        get() = NativeConfig.getIsSaveable(key)

    fun getValueAsString(needsGlobal: Boolean = false): String

    fun reset()
}
