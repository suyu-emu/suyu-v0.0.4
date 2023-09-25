// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

class RunnableSetting(
    titleId: Int,
    descriptionId: Int,
    val isRuntimeRunnable: Boolean,
    val runnable: () -> Unit
) : SettingsItem(emptySetting, titleId, descriptionId) {
    override val type = TYPE_RUNNABLE
}
