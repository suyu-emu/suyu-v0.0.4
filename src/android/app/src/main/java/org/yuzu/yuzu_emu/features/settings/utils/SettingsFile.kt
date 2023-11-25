// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.utils

import java.io.*
import org.yuzu.yuzu_emu.utils.DirectoryInitialization

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config"

    fun getSettingsFile(fileName: String): File =
        File(DirectoryInitialization.userDirectory + "/config/" + fileName + ".ini")
}
