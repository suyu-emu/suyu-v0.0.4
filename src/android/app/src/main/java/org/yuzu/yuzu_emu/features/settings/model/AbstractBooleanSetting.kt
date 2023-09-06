// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

interface AbstractBooleanSetting : AbstractSetting {
    val boolean: Boolean

    fun setBoolean(value: Boolean)
}
