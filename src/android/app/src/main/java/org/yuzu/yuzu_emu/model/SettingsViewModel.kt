// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.SavedStateHandle
import androidx.lifecycle.ViewModel
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem

class SettingsViewModel(private val savedStateHandle: SavedStateHandle) : ViewModel() {
    var game: Game? = null

    var shouldSave = false

    var clickedItem: SettingsItem? = null

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

    private val _isUsingSearch = MutableLiveData(false)
    val isUsingSearch: LiveData<Boolean> get() = _isUsingSearch

    val sliderProgress = savedStateHandle.getStateFlow(KEY_SLIDER_PROGRESS, -1)

    val sliderTextValue = savedStateHandle.getStateFlow(KEY_SLIDER_TEXT_VALUE, "")

    val adapterItemChanged = savedStateHandle.getStateFlow(KEY_ADAPTER_ITEM_CHANGED, -1)

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

    fun setIsUsingSearch(value: Boolean) {
        _isUsingSearch.value = value
    }

    fun setSliderTextValue(value: Float, units: String) {
        savedStateHandle[KEY_SLIDER_PROGRESS] = value
        savedStateHandle[KEY_SLIDER_TEXT_VALUE] = String.format(
            YuzuApplication.appContext.getString(R.string.value_with_units),
            value.toInt().toString(),
            units
        )
    }

    fun setSliderProgress(value: Float) {
        savedStateHandle[KEY_SLIDER_PROGRESS] = value
    }

    fun setAdapterItemChanged(value: Int) {
        savedStateHandle[KEY_ADAPTER_ITEM_CHANGED] = value
    }

    fun clear() {
        game = null
        shouldSave = false
    }

    companion object {
        const val KEY_SLIDER_TEXT_VALUE = "SliderTextValue"
        const val KEY_SLIDER_PROGRESS = "SliderProgress"
        const val KEY_ADAPTER_ITEM_CHANGED = "AdapterItemChanged"
    }
}
