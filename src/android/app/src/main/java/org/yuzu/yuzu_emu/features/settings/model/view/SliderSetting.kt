package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.FloatSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.utils.Log
import kotlin.math.roundToInt

class SliderSetting(
    key: String,
    section: String,
    setting: Setting?,
    titleId: Int,
    descriptionId: Int,
    val min: Int,
    val max: Int,
    val units: String,
    val defaultValue: Int,
) : SettingsItem(key, section, setting, titleId, descriptionId) {
    override val type = TYPE_SLIDER

    val selectedValue: Int
        get() {
            val setting = setting ?: return defaultValue
            return when (setting) {
                is IntSetting -> setting.value
                is FloatSetting -> setting.value.roundToInt()
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

    /**
     * Write a value to the backing float. If that float was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param selection New value of the float.
     * @return null if overwritten successfully otherwise; a newly created FloatSetting.
     */
    fun setSelectedValue(selection: Float): FloatSetting? {
        return if (setting == null) {
            val newSetting = FloatSetting(key!!, section!!, selection)
            setting = newSetting
            newSetting
        } else {
            val newSetting = setting as FloatSetting
            newSetting.value = selection
            null
        }
    }
}
