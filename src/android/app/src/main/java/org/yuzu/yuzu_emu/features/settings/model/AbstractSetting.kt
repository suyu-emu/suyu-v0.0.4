// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

import org.yuzu.yuzu_emu.utils.NativeConfig

interface AbstractSetting {
    val key: String
    val category: Settings.Category
    val defaultValue: Any
    val androidDefault: Any?
        get() = null
    val valueAsString: String
        get() = ""

    val isRuntimeModifiable: Boolean
        get() = NativeConfig.getIsRuntimeModifiable(key)

    val pairedSettingKey: String
        get() = NativeConfig.getPairedSettingKey(key)

    fun reset()
}
