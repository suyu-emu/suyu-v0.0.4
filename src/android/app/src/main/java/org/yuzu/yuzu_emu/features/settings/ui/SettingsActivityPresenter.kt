// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.os.Bundle
import android.text.TextUtils
import java.io.File
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.Log

class SettingsActivityPresenter(private val activityView: SettingsActivityView) {
    val settings: Settings get() = activityView.settings

    private var shouldSave = false
    private lateinit var menuTag: String
    private lateinit var gameId: String

    fun onCreate(savedInstanceState: Bundle?, menuTag: String, gameId: String) {
        this.menuTag = menuTag
        this.gameId = gameId
        if (savedInstanceState != null) {
            shouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE)
        }
    }

    fun onStart() {
        prepareDirectoriesIfNeeded()
    }

    private fun loadSettingsUI() {
        if (!settings.isLoaded) {
            if (!TextUtils.isEmpty(gameId)) {
                settings.loadSettings(gameId, activityView)
            } else {
                settings.loadSettings(activityView)
            }
        }
        activityView.showSettingsFragment(menuTag, false, gameId)
        activityView.onSettingsFileLoaded()
    }

    private fun prepareDirectoriesIfNeeded() {
        val configFile =
            File(
                "${DirectoryInitialization.userDirectory}/config/" +
                    "${SettingsFile.FILE_NAME_CONFIG}.ini"
            )
        if (!configFile.exists()) {
            Log.error(
                "${DirectoryInitialization.userDirectory}/config/" +
                    "${SettingsFile.FILE_NAME_CONFIG}.ini"
            )
            Log.error("yuzu config file could not be found!")
        }

        if (!DirectoryInitialization.areDirectoriesReady) {
            DirectoryInitialization.start(activityView as Context)
        }
        loadSettingsUI()
    }

    fun onStop(finishing: Boolean) {
        if (finishing && shouldSave) {
            Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...")
            settings.saveSettings(activityView)
        }
        NativeLibrary.reloadSettings()
    }

    fun onSettingChanged() {
        shouldSave = true
    }

    fun onSettingsReset() {
        shouldSave = false
    }

    fun saveState(outState: Bundle) {
        outState.putBoolean(KEY_SHOULD_SAVE, shouldSave)
    }

    companion object {
        private const val KEY_SHOULD_SAVE = "should_save"
    }
}
