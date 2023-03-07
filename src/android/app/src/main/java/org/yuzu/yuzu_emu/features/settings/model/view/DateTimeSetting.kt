package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.features.settings.model.StringSetting

class DateTimeSetting(
    key: String,
    section: String,
    titleId: Int,
    descriptionId: Int,
    private val defaultValue: String,
    setting: Setting
) : SettingsItem(key, section, setting, titleId, descriptionId) {
    override val type = TYPE_DATETIME_SETTING

    val value: String
        get() = if (setting != null) {
            val setting = setting as StringSetting
            setting.value
        } else {
            defaultValue
        }

    fun setSelectedValue(datetime: String): StringSetting? {
        return if (setting == null) {
            val newSetting = StringSetting(key!!, section!!, datetime)
            setting = newSetting
            newSetting
        } else {
            val newSetting = setting as StringSetting
            newSetting.value = datetime
            null
        }
    }
}
