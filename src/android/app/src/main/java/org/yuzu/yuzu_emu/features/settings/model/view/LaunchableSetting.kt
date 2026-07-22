// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import android.content.Intent
import androidx.annotation.StringRes

/**
 * A settings item that launches an intent when clicked.
 */
class LaunchableSetting(
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    val launchIntent: (android.content.Context) -> Intent
) : SettingsItem(emptySetting, titleId, titleString, descriptionId, descriptionString) {
    override val type = SettingsItem.TYPE_LAUNCHABLE
}
