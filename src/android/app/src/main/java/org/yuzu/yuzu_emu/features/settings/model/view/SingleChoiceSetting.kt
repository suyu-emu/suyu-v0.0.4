// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.ArrayRes
import androidx.annotation.StringRes
import org.yuzu.yuzu_emu.features.settings.model.AbstractByteSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting

class SingleChoiceSetting(
    setting: AbstractSetting,
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    @ArrayRes val choicesId: Int,
    @ArrayRes val valuesId: Int,
    val warnChoices: List<Int> = ArrayList(),
    @StringRes val warningMessage: Int = 0
) : SettingsItem(setting, titleId, titleString, descriptionId, descriptionString) {
    override val type = TYPE_SINGLE_CHOICE

    fun getSelectedValue(needsGlobal: Boolean = false) =
        when (setting) {
            is AbstractIntSetting -> setting.getInt(needsGlobal)
            is AbstractByteSetting -> setting.getByte(needsGlobal).toInt()
            else -> -1
        }

    fun setSelectedValue(value: Int) =
        when (setting) {
            is AbstractIntSetting -> setting.setInt(value)
            is AbstractByteSetting -> setting.setByte(value.toByte())
            else -> -1
        }
}
