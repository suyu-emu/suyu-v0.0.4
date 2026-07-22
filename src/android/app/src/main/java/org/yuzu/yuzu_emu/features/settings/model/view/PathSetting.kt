// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.model.view

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes

class PathSetting(
    @StringRes titleId: Int = 0,
    titleString: String = "",
    @StringRes descriptionId: Int = 0,
    descriptionString: String = "",
    @DrawableRes val iconId: Int = 0,
    val pathType: PathType,
    val defaultPathGetter: () -> String,
    val currentPathGetter: () -> String,
    val pathSetter: (String) -> Unit
) : SettingsItem(emptySetting, titleId, titleString, descriptionId, descriptionString) {

    override val type = TYPE_PATH

    enum class PathType {
        SAVE_DATA,
        NAND,
        SDMC
    }

    fun getCurrentPath(): String = currentPathGetter()

    fun getDefaultPath(): String = defaultPathGetter()

    fun setPath(path: String) = pathSetter(path)

    fun isUsingDefaultPath(): Boolean = getCurrentPath() == getDefaultPath()

    companion object {
        const val TYPE_PATH = 14
    }
}
