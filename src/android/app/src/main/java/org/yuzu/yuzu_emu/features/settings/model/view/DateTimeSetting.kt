// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractLongSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting

class DateTimeSetting(
    setting: AbstractSetting?,
    titleId: Int,
    descriptionId: Int,
    val key: String? = null,
    private val defaultValue: Long? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_DATETIME_SETTING

    val value: Long
        get() = if (setting != null) {
            val setting = setting as AbstractLongSetting
            setting.long
        } else {
            defaultValue!!
        }

    fun setSelectedValue(datetime: Long): AbstractLongSetting {
        val longSetting = setting as AbstractLongSetting
        longSetting.setLong(datetime)
        return longSetting
    }
}
