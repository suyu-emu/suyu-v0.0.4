package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.features.settings.model.StringSetting

class StringSingleChoiceSetting(
    key: String,
    section: String,
    setting: Setting?,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Array<String>,
    private val valuesId: Array<String>?,
    private val defaultValue: String
) : SettingsItem(key, section, setting, titleId, descriptionId) {
    override val type = TYPE_STRING_SINGLE_CHOICE

    fun getValueAt(index: Int): String? {
        if (valuesId == null) return null
        return if (index >= 0 && index < valuesId.size) {
            valuesId[index]
        } else ""
    }

    val selectedValue: String
        get() = if (setting != null) {
            val setting = setting as StringSetting
            setting.value
        } else {
            defaultValue
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
     * @return null if overwritten successfully otherwise; a newly created IntSetting.
     */
    fun setSelectedValue(selection: String?): StringSetting? {
        return if (setting == null) {
            val newSetting = StringSetting(key!!, section!!, selection!!)
            setting = newSetting
            newSetting
        } else {
            val newSetting = setting as StringSetting
            newSetting.value = selection!!
            null
        }
    }
}
