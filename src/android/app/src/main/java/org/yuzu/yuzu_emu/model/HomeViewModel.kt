// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow

class HomeViewModel : ViewModel() {
    val navigationVisible: StateFlow<Pair<Boolean, Boolean>> get() = _navigationVisible
    private val _navigationVisible = MutableStateFlow(Pair(false, false))

    val statusBarShadeVisible: StateFlow<Boolean> get() = _statusBarShadeVisible
    private val _statusBarShadeVisible = MutableStateFlow(true)

    val shouldPageForward: StateFlow<Boolean> get() = _shouldPageForward
    private val _shouldPageForward = MutableStateFlow(false)

    private val _gamesDirSelected = MutableStateFlow(false)
    val gamesDirSelected get() = _gamesDirSelected.asStateFlow()

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

    fun setGamesDirSelected(selected: Boolean) {
        _gamesDirSelected.value = selected
    }
}
