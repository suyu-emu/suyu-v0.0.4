// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.AbstractStringSetting
import org.yuzu.yuzu_emu.features.settings.model.StringSetting

class StringSingleChoiceSetting(
    val key: String? = null,
    setting: AbstractSetting?,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Array<String>,
    private val valuesId: Array<String>?,
    private val defaultValue: String? = null
) : SettingsItem(setting, titleId, descriptionId) {
    override val type = TYPE_STRING_SINGLE_CHOICE

    fun getValueAt(index: Int): String? {
        if (valuesId == null) return null
        return if (index >= 0 && index < valuesId.size) {
            valuesId[index]
        } else ""
    }

    val selectedValue: String
        get() = if (setting != null) {
            val setting = setting as AbstractStringSetting
            setting.string
        } else {
            defaultValue!!
        }
    val selectValueIndex: Int
        get() {
            val selectedValue = selectedValue
            for (i in valuesId!!.indices) {
                if (valuesId[i] == selectedValue) {
                    return i
                }
            }
            return -1
        }

    /**
     * Write a value to the backing int. If that int was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the int.
     * @return the existing setting with the new value applied.
     */
    fun setSelectedValue(selection: String): AbstractStringSetting {
        val stringSetting = setting as AbstractStringSetting
        stringSetting.string = selection
        return stringSetting
    }
}
