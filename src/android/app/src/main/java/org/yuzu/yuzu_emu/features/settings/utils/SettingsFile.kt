// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.utils

import android.widget.Toast
import java.io.*
import org.ini4j.Wini
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.*
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.NativeConfig

/**
 * Contains static methods for interacting with .ini files in which settings are stored.
 */
object SettingsFile {
    const val FILE_NAME_CONFIG = "config"

    /**
     * Saves a Settings HashMap to a given .ini file on disk. If unsuccessful, outputs an error
     * telling why it failed.
     *
     * @param fileName The target filename without a path or extension.
     */
    fun saveFile(fileName: String) {
        val ini = getSettingsFile(fileName)
        try {
            val wini = Wini(ini)
            for (specificCategory in Settings.Category.values()) {
                val categoryHeader = NativeConfig.getConfigHeader(specificCategory.ordinal)
                for (setting in Settings.settingsList) {
                    if (setting.key!!.isEmpty()) continue

                    val settingCategoryHeader =
                        NativeConfig.getConfigHeader(setting.category.ordinal)
                    val iniSetting: String? = wini.get(categoryHeader, setting.key)
                    if (iniSetting != null || settingCategoryHeader == categoryHeader) {
                        wini.put(settingCategoryHeader, setting.key, setting.valueAsString)
                    }
                }
            }
            wini.store()
        } catch (e: IOException) {
            Log.error("[SettingsFile] File not found: " + fileName + ".ini: " + e.message)
            val context = YuzuApplication.appContext
            Toast.makeText(
                context,
                context.getString(R.string.error_saving, fileName, e.message),
                Toast.LENGTH_SHORT
            ).show()
        }
    }

    fun getSettingsFile(fileName: String): File =
        File(DirectoryInitialization.userDirectory + "/config/" + fileName + ".ini")
}
