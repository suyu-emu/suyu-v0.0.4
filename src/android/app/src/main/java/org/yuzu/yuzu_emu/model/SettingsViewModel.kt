// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class SettingsViewModel : ViewModel() {
    var game: Game? = null

    var shouldSave = false

    private val _toolbarTitle = MutableLiveData("")
    val toolbarTitle: LiveData<String> get() = _toolbarTitle

    private val _shouldRecreate = MutableLiveData(false)
    val shouldRecreate: LiveData<Boolean> get() = _shouldRecreate

    private val _shouldNavigateBack = MutableLiveData(false)
    val shouldNavigateBack: LiveData<Boolean> get() = _shouldNavigateBack

    private val _shouldShowResetSettingsDialog = MutableLiveData(false)
    val shouldShowResetSettingsDialog: LiveData<Boolean> get() = _shouldShowResetSettingsDialog

    private val _shouldReloadSettingsList = MutableLiveData(false)
    val shouldReloadSettingsList: LiveData<Boolean> get() = _shouldReloadSettingsList

    fun setToolbarTitle(value: String) {
        _toolbarTitle.value = value
    }

    fun setShouldRecreate(value: Boolean) {
        _shouldRecreate.value = value
    }

    fun setShouldNavigateBack(value: Boolean) {
        _shouldNavigateBack.value = value
    }

    fun setShouldShowResetSettingsDialog(value: Boolean) {
        _shouldShowResetSettingsDialog.value = value
    }

    fun setShouldReloadSettingsList(value: Boolean) {
        _shouldReloadSettingsList.value = value
    }

    fun clear() {
        game = null
        shouldSave = false
    }
}
