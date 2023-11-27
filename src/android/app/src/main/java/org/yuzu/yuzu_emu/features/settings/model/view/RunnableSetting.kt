// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.DrawableRes

class RunnableSetting(
    titleId: Int,
    descriptionId: Int,
    val isRuntimeRunnable: Boolean,
    @DrawableRes val iconId: Int = 0,
    val runnable: () -> Unit
) : SettingsItem(emptySetting, titleId, descriptionId) {
    override val type = TYPE_RUNNABLE
}
