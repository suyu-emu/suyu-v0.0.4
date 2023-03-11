// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import org.yuzu.yuzu_emu.features.settings.model.Setting

/**
 * ViewModel abstraction for an Item in the RecyclerView powering SettingsFragments.
 * Each one corresponds to a [Setting] object, so this class's subclasses
 * should vaguely correspond to those subclasses. There are a few with multiple analogues
 * and a few with none (Headers, for example, do not correspond to anything in the ini
 * file.)
 */
abstract class SettingsItem(
    val key: String?,
    val section: String?,
    var setting: Setting?,
    val nameId: Int,
    val descriptionId: Int?
) {
    abstract val type: Int

    companion object {
        const val TYPE_HEADER = 0
        const val TYPE_CHECKBOX = 1
        const val TYPE_SINGLE_CHOICE = 2
        const val TYPE_SLIDER = 3
        const val TYPE_SUBMENU = 4
        const val TYPE_STRING_SINGLE_CHOICE = 5
        const val TYPE_DATETIME_SETTING = 6
    }
}
