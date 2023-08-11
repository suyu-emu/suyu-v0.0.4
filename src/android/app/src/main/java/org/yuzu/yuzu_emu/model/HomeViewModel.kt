// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class HomeViewModel : ViewModel() {
    private val _navigationVisible = MutableLiveData<Pair<Boolean, Boolean>>()
    val navigationVisible: LiveData<Pair<Boolean, Boolean>> get() = _navigationVisible

    private val _statusBarShadeVisible = MutableLiveData(true)
    val statusBarShadeVisible: LiveData<Boolean> get() = _statusBarShadeVisible

    private val _shouldPageForward = MutableLiveData(false)
    val shouldPageForward: LiveData<Boolean> get() = _shouldPageForward

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
}
