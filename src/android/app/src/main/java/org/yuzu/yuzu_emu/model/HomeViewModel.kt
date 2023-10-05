// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import android.net.Uri
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.preference.PreferenceManager
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.GameHelper

class HomeViewModel : ViewModel() {
    val navigationVisible: StateFlow<Pair<Boolean, Boolean>> get() = _navigationVisible
    private val _navigationVisible = MutableStateFlow(Pair(false, false))

    val statusBarShadeVisible: StateFlow<Boolean> get() = _statusBarShadeVisible
    private val _statusBarShadeVisible = MutableStateFlow(true)

    val shouldPageForward: StateFlow<Boolean> get() = _shouldPageForward
    private val _shouldPageForward = MutableStateFlow(false)

    val gamesDir: StateFlow<String> get() = _gamesDir
    private val _gamesDir = MutableStateFlow(
        Uri.parse(
            PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
                .getString(GameHelper.KEY_GAME_PATH, "")
        ).path ?: ""
    )

    var navigatedToSetup = false

    fun setNavigationVisibility(visible: Boolean, animated: Boolean) {
        if (navigationVisible.value.first == visible) {
            return
        }
        _navigationVisible.value = Pair(visible, animated)
    }

    fun setStatusBarShadeVisibility(visible: Boolean) {
        if (statusBarShadeVisible.value == visible) {
            return
        }
        _statusBarShadeVisible.value = visible
    }

    fun setShouldPageForward(pageForward: Boolean) {
        _shouldPageForward.value = pageForward
    }

    fun setGamesDir(activity: FragmentActivity, dir: String) {
        ViewModelProvider(activity)[GamesViewModel::class.java].reloadGames(true)
        _gamesDir.value = dir
    }
}
