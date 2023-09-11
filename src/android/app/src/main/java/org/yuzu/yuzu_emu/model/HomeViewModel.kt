// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import android.net.Uri
import androidx.fragment.app.FragmentActivity
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.ViewModelProvider
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.GameHelper

class HomeViewModel : ViewModel() {
    private val _navigationVisible = MutableLiveData<Pair<Boolean, Boolean>>()
    val navigationVisible: LiveData<Pair<Boolean, Boolean>> get() = _navigationVisible

    private val _statusBarShadeVisible = MutableLiveData(true)
    val statusBarShadeVisible: LiveData<Boolean> get() = _statusBarShadeVisible

    private val _shouldPageForward = MutableLiveData(false)
    val shouldPageForward: LiveData<Boolean> get() = _shouldPageForward

    private val _gamesDir = MutableLiveData(
        Uri.parse(
            PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
                .getString(GameHelper.KEY_GAME_PATH, "")
        ).path ?: ""
    )
    val gamesDir: LiveData<String> get() = _gamesDir

    var navigatedToSetup = false

    init {
        _navigationVisible.value = Pair(false, false)
    }

    fun setNavigationVisibility(visible: Boolean, animated: Boolean) {
        if (_navigationVisible.value?.first == visible) {
            return
        }
        _navigationVisible.value = Pair(visible, animated)
    }

    fun setStatusBarShadeVisibility(visible: Boolean) {
        if (_statusBarShadeVisible.value == visible) {
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
