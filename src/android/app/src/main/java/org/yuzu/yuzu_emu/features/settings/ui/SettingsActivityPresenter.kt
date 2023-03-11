// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.IntentFilter
import android.os.Bundle
import android.text.TextUtils
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.DirectoryInitialization.DirectoryInitializationState
import org.yuzu.yuzu_emu.utils.DirectoryStateReceiver
import org.yuzu.yuzu_emu.utils.Log
import java.io.File

class SettingsActivityPresenter(private val activityView: SettingsActivityView) {
    var settings: Settings? = Settings()
    private var shouldSave = false
    private var directoryStateReceiver: DirectoryStateReceiver? = null
    private lateinit var menuTag: String
    private lateinit var gameId: String

    fun onCreate(savedInstanceState: Bundle?, menuTag: String, gameId: String) {
        if (savedInstanceState == null) {
            this.menuTag = menuTag
            this.gameId = gameId
        } else {
            shouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE)
        }
    }

    fun onStart() {
        prepareDirectoriesIfNeeded()
    }

    private fun loadSettingsUI() {
        if (settings!!.isEmpty) {
            if (!TextUtils.isEmpty(gameId)) {
                settings!!.loadSettings(gameId, activityView)
            } else {
                settings!!.loadSettings(activityView)
            }
        }
        activityView.showSettingsFragment(menuTag, false, gameId)
        activityView.onSettingsFileLoaded(settings)
    }

    private fun prepareDirectoriesIfNeeded() {
        val configFile =
            File(DirectoryInitialization.userDirectory + "/config/" + SettingsFile.FILE_NAME_CONFIG + ".ini")
        if (!configFile.exists()) {
            Log.error(DirectoryInitialization.userDirectory + "/config/" + SettingsFile.FILE_NAME_CONFIG + ".ini")
            Log.error("yuzu config file could not be found!")
        }
        if (DirectoryInitialization.areDirectoriesReady()) {
            loadSettingsUI()
        } else {
            activityView.showLoading()
            val statusIntentFilter = IntentFilter(DirectoryInitialization.BROADCAST_ACTION)
            directoryStateReceiver =
                DirectoryStateReceiver { directoryInitializationState: DirectoryInitializationState ->
                    if (directoryInitializationState == DirectoryInitializationState.YUZU_DIRECTORIES_INITIALIZED) {
                        activityView.hideLoading()
                        loadSettingsUI()
                    } else if (directoryInitializationState == DirectoryInitializationState.CANT_FIND_EXTERNAL_STORAGE) {
                        activityView.showExternalStorageNotMountedHint()
                        activityView.hideLoading()
                    }
                }
            activityView.startDirectoryInitializationService(
                directoryStateReceiver,
                statusIntentFilter
            )
        }
    }

    fun onStop(finishing: Boolean) {
        if (directoryStateReceiver != null) {
            activityView.stopListeningToDirectoryInitializationService(directoryStateReceiver!!)
            directoryStateReceiver = null
        }
        if (settings != null && finishing && shouldSave) {
            Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...")
            settings!!.saveSettings(activityView)
        }
        NativeLibrary.ReloadSettings()
    }

    fun onSettingChanged() {
        shouldSave = true
    }

    fun saveState(outState: Bundle) {
        outState.putBoolean(KEY_SHOULD_SAVE, shouldSave)
    }

    companion object {
        private const val KEY_SHOULD_SAVE = "should_save"
    }
}
