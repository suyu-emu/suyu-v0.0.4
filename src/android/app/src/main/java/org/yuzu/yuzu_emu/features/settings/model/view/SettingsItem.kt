// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.features.settings.model.AbstractSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a [AbstractSetting] object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
abstract class SettingsItem(
    val setting: AbstractSetting,
    val nameId: Int,
    val descriptionId: Int
) {
    abstract val type: Int

    val isEditable: Boolean
        get() {
            if (!NativeLibrary.isRunning()) return true
            return setting.isRuntimeModifiable
        }

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_SWITCH = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_STRING_SINGLE_CHOICE = 5
        const val TYPE_DATETIME_SETTING = 6
        const val TYPE_RUNNABLE = 7

        val emptySetting = object : AbstractSetting {
            override val key: String = ""
            override val category: Settings.Category = Settings.Category.Ui
            override val defaultValue: Any = false
            override fun reset() {}
        }
    }
}
