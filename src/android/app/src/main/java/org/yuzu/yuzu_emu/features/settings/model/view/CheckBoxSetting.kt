// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.features.settings.ui.SettingsFragmentView

class CheckBoxSetting : SettingsItem {
    override val type = TYPE_CHECKBOX

    private var defaultValue: Boolean
    private var showPerformanceWarning: Boolean
    private var fragmentView: SettingsFragmentView? = null

    constructor(
        key: String,
        section: String,
        setting: Setting?,
        titleId: Int,
        descriptionId: Int,
        defaultValue: Boolean
    ) : super(key, section, setting, titleId, descriptionId) {
        this.defaultValue = defaultValue
        showPerformanceWarning = false
    }

    constructor(
        key: String,
        section: String,
        titleId: Int,
        descriptionId: Int,
        defaultValue: Boolean,
        setting: Setting,
        show_performance_warning: Boolean,
        view: SettingsFragmentView
    ) : super(key, section, setting, titleId, descriptionId) {
        this.defaultValue = defaultValue
        fragmentView = view
        showPerformanceWarning = show_performance_warning
    }

    val isChecked: Boolean
        get() {
            if (setting == null) {
                return defaultValue
            }

            // Try integer setting
            try {
                val setting = setting as IntSetting
                return setting.value == 1
            } catch (_: ClassCastException) {
            }

            // Try boolean setting
            try {
                val setting = setting as BooleanSetting
                return setting.value
            } catch (_: ClassCastException) {
            }
            return defaultValue
        }

    /**
     * Write a value to the backing boolean. If that boolean was previously null,
     * initializes a new one and returns it, so it can be added to the Hashmap.
     *
     * @param checked Pretty self explanatory.
     * @return null if overwritten successfully; otherwise, a newly created BooleanSetting.
     */
    fun setChecked(checked: Boolean): IntSetting? {
        // Show a performance warning if the setting has been disabled
        if (showPerformanceWarning && !checked) {
            fragmentView!!.showToastMessage(
                YuzuApplication.appContext.getString(R.string.performance_warning), true
            )
        }

        return if (setting == null) {
            val newSetting = IntSetting(key!!, section!!, if (checked) 1 else 0)
            setting = newSetting
            newSetting
        } else {
            val newSetting = setting as IntSetting
            newSetting.value = if (checked) 1 else 0
            null
        }
    }
}
