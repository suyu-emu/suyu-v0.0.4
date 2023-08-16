// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractByteSetting
import kotlin.math.roundToInt
import org.yuzu.yuzu_emu.features.settings.model.AbstractFloatSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractIntSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractShortSetting
import org.yuzu.yuzu_emu.utils.Log

class SliderSetting(
    setting: AbstractSetting?,
    titleId: Int,
    descriptionId: Int,
    val min: Int,
    val max: Int,
    val units: String,
    val key: String? = null,
    val defaultValue: Any? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_SLIDER

    val selectedValue: Any
        get() {
            val setting = setting ?: return defaultValue!!
            return when (setting) {
                is AbstractByteSetting -> setting.byte.toInt()
                is AbstractShortSetting -> setting.short.toInt()
                is AbstractIntSetting -> setting.int
                is AbstractFloatSetting -> setting.float.roundToInt()
                else -> {
                    Log.error("[SliderSetting] Error casting setting type.")
                    -1
                }
            }
        }

    /**
     * Write a value to the backing int. If that int was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the int.
     * @return the existing setting with the new value applied.
     */
    fun setSelectedValue(selection: Int): AbstractIntSetting {
        val intSetting = setting as AbstractIntSetting
        intSetting.setInt(selection)
        return intSetting
    }

    /**
     * Write a value to the backing float. If that float was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the float.
     * @return the existing setting with the new value applied.
     */
    fun setSelectedValue(selection: Float): AbstractFloatSetting {
        val floatSetting = setting as AbstractFloatSetting
        floatSetting.setFloat(selection)
        return floatSetting
    }

    fun setSelectedValue(selection: Short): AbstractShortSetting {
        val shortSetting = setting as AbstractShortSetting
        shortSetting.setShort(selection)
        return shortSetting
    }

    fun setSelectedValue(selection: Byte): AbstractByteSetting {
        val byteSetting = setting as AbstractByteSetting
        byteSetting.setByte(selection)
        return byteSetting
    }
}
