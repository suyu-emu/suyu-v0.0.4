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

    var selectedValue: Int
        get() {
            return when (setting) {
                is AbstractIntSetting -> setting.int
                else -> -1
            }
        }
        set(value) {
            when (setting) {
                is AbstractIntSetting -> setting.setInt(value)
            }
        }
}
