// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.StringRes
import org.yuzu.yuzu_emu.features.settings.model.AbstractStringSetting

class StringInputSetting(
    setting: AbstractStringSetting,
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    val onGenerate: (() -> String)? = null,
    val validator: ((s: String?) -> Boolean)? = null,
    @StringRes val errorId: Int = 0
) : SettingsItem(setting, titleId, titleString, descriptionId, descriptionString) {
    override val type = TYPE_STRING_INPUT

    fun getSelectedValue(needsGlobal: Boolean = false) = setting.getValueAsString(needsGlobal)

    fun setSelectedValue(selection: String) =
        (setting as AbstractStringSetting).setString(selection)
}
