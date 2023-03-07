package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Setting

class SingleChoiceSetting(
    key: String,
    section: String,
    setting: Setting?,
    titleId: Int,
    descriptionId: Int,
    val choicesId: Int,
    val valuesId: Int,
    private val defaultValue: Int,
) : SettingsItem(key, section, setting, titleId, descriptionId) {
    override val type = TYPE_SINGLE_CHOICE

    val selectedValue: Int
        get() = if (setting != null) {
            val setting = setting as IntSetting
            setting.value
        } else {
            defaultValue
        }

    /**
     * Write a value to the backing int. If that int was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the int.
     * @return null if overwritten successfully otherwise; a newly created IntSetting.
     */
    fun setSelectedValue(selection: Int): IntSetting? {
        return if (setting == null) {
            val newSetting = IntSetting(key!!, section!!, selection)
            setting = newSetting
            newSetting
        } else {
            val newSetting = setting as IntSetting
            newSetting.value = selection
            null
        }
    }
}
