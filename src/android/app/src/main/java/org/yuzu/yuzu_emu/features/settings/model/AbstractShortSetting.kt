// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model

interface AbstractShortSetting : AbstractSetting {
    val short: Short

    fun setShort(value: Short)
}
