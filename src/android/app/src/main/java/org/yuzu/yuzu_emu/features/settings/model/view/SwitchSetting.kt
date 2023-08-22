// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractBooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting

class SwitchSetting(
    setting: AbstractSetting,
    titleId: Int,
    descriptionId: Int
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SWITCH

    var checked: Boolean
        get() {
            return when (setting) {
                is AbstractIntSetting -> setting.int == 1
                is AbstractBooleanSetting -> setting.boolean
                else -> false
            }
        }
        set(value) {
            when (setting) {
                is AbstractIntSetting -> setting.setInt(if (value) 1 else 0)
                is AbstractBooleanSetting -> setting.setBoolean(value)
            }
        }
}
