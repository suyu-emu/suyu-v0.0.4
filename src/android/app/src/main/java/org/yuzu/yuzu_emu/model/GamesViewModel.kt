// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.GameHelper

class GamesViewModel : ViewModel() {
    private val _games = MutableLiveData<List<Game>>(emptyList())
    val games: LiveData<List<Game>> get() = _games

    private val _searchedGames = MutableLiveData<List<Game>>(emptyList())
    val searchedGames: LiveData<List<Game>> get() = _searchedGames

    private val _isReloading = MutableLiveData(false)
    val isReloading: LiveData<Boolean> get() = _isReloading

    private val _shouldSwapData = MutableLiveData(false)
    val shouldSwapData: LiveData<Boolean> get() = _shouldSwapData

    private val _shouldScrollToTop = MutableLiveData(false)
    val shouldScrollToTop: LiveData<Boolean> get() = _shouldScrollToTop

    private val _searchFocused = MutableLiveData(false)
    val searchFocused: LiveData<Boolean> get() = _searchFocused

    init {
        reloadGames(false)
    }

    fun setSearchedGames(games: List<Game>) {
        _searchedGames.postValue(games)
    }

    fun setShouldSwapData(shouldSwap: Boolean) {
        _shouldSwapData.postValue(shouldSwap)
    }

    fun setShouldScrollToTop(shouldScroll: Boolean) {
        _shouldScrollToTop.postValue(shouldScroll)
    }

    fun setSearchFocused(searchFocused: Boolean) {
        _searchFocused.postValue(searchFocused)
    }

    fun reloadGames(directoryChanged: Boolean) {
        if (isReloading.value == true)
            return
        _isReloading.postValue(true)

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                NativeLibrary.resetRomMetadata()
                _games.postValue(GameHelper.getGames())
                _isReloading.postValue(false)

                if (directoryChanged) {
                    setShouldSwapData(true)
                }
            }
        }
    }
}
