// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import androidx.preference.PreferenceManager
import java.io.IOException
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.utils.PreferenceUtil.migratePreference

object DirectoryInitialization {
    private var userPath: String? = null

    var areDirectoriesReady: Boolean = false

    fun start() {
        if (!areDirectoriesReady) {
            initializeInternalStorage()
            NativeLibrary.initializeSystem(false)
            NativeConfig.initializeGlobalConfig()
            migrateSettings()
            areDirectoriesReady = true
        }
    }

    val userDirectory: String?
        get() {
            check(areDirectoriesReady) { "Directory initialization is not ready!" }
            return userPath
        }

    private fun initializeInternalStorage() {
        try {
            userPath = YuzuApplication.appContext.getExternalFilesDir(null)!!.canonicalPath
            NativeLibrary.setAppDirectory(userPath!!)
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }

    private fun migrateSettings() {
        val preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        var saveConfig = false
        val theme = preferences.migratePreference<Int>(Settings.PREF_THEME)
        if (theme != null) {
            IntSetting.THEME.setInt(theme)
            saveConfig = true
        }

        val themeMode = preferences.migratePreference<Int>(Settings.PREF_THEME_MODE)
        if (themeMode != null) {
            IntSetting.THEME_MODE.setInt(themeMode)
            saveConfig = true
        }

        val blackBackgrounds =
            preferences.migratePreference<Boolean>(Settings.PREF_BLACK_BACKGROUNDS)
        if (blackBackgrounds != null) {
            BooleanSetting.BLACK_BACKGROUNDS.setBoolean(blackBackgrounds)
            saveConfig = true
        }

        if (saveConfig) {
            NativeConfig.saveGlobalConfig()
        }
    }
}
