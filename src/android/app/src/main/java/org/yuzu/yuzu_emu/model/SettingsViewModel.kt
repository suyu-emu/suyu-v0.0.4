// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem

class SettingsViewModel : ViewModel() {
    var game: Game? = null

    var clickedItem: SettingsItem? = null

    val shouldRecreate: StateFlow<Boolean> get() = _shouldRecreate
    private val _shouldRecreate = MutableStateFlow(false)

    val shouldNavigateBack: StateFlow<Boolean> get() = _shouldNavigateBack
    private val _shouldNavigateBack = MutableStateFlow(false)

    val shouldShowResetSettingsDialog: StateFlow<Boolean> get() = _shouldShowResetSettingsDialog
    private val _shouldShowResetSettingsDialog = MutableStateFlow(false)

    val shouldReloadSettingsList: StateFlow<Boolean> get() = _shouldReloadSettingsList
    private val _shouldReloadSettingsList = MutableStateFlow(false)

    val sliderProgress: StateFlow<Int> get() = _sliderProgress
    private val _sliderProgress = MutableStateFlow(-1)

    val sliderTextValue: StateFlow<String> get() = _sliderTextValue
    private val _sliderTextValue = MutableStateFlow("")

    val adapterItemChanged: StateFlow<Int> get() = _adapterItemChanged
    private val _adapterItemChanged = MutableStateFlow(-1)

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

    fun setSliderTextValue(value: Float, units: String) {
        _sliderProgress.value = value.toInt()
        _sliderTextValue.value = String.format(
            YuzuApplication.appContext.getString(R.string.value_with_units),
            value.toInt().toString(),
            units
        )
    }

    fun setSliderProgress(value: Float) {
        _sliderProgress.value = value.toInt()
    }

    fun setAdapterItemChanged(value: Int) {
        _adapterItemChanged.value = value
    }
}
