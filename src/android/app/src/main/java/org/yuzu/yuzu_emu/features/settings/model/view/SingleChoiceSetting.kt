// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting

class SingleChoiceSetting(
    setting: AbstractSetting,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Int,
    val valuesId: Int
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SINGLE_CHOICE

    fun getSelectedValue(needsGlobal: Boolean = false) =
        when (setting) {
            is AbstractIntSetting -> setting.getInt(needsGlobal)
            else -> -1
        }

    fun setSelectedValue(value: Int) = (setting as AbstractIntSetting).setInt(value)
}
