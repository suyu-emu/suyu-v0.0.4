// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractByteSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractFloatSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractShortSetting
import kotlin.math.roundToInt

class SliderSetting(
    setting: AbstractSetting,
    titleId: Int,
    descriptionId: Int,
    val min: Int,
    val max: Int,
    val units: String
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SLIDER

    var selectedValue: Int
        get() {
            return when (setting) {
                is AbstractByteSetting -> setting.byte.toInt()
                is AbstractShortSetting -> setting.short.toInt()
                is AbstractIntSetting -> setting.int
                is AbstractFloatSetting -> setting.float.roundToInt()
                else -> -1
            }
        }
        set(value) {
            when (setting) {
                is AbstractByteSetting -> setting.setByte(value.toByte())
                is AbstractShortSetting -> setting.setShort(value.toShort())
                is AbstractIntSetting -> setting.setInt(value)
                is AbstractFloatSetting -> setting.setFloat(value.toFloat())
            }
        }
}
