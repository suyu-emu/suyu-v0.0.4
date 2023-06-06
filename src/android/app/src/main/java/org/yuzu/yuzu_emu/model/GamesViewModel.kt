// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import androidx.preference.PreferenceManager
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.json.Json
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.GameHelper
import java.util.Locale

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
        // Ensure keys are loaded so that ROM metadata can be decrypted.
        NativeLibrary.reloadKeys()

        // Retrieve list of cached games
        val storedGames = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
            .getStringSet(GameHelper.KEY_GAMES, emptySet())
        if (storedGames!!.isNotEmpty()) {
            val deserializedGames = mutableSetOf<Game>()
            storedGames.forEach {
                val game: Game = Json.decodeFromString(it)
                val gameExists =
                    DocumentFile.fromSingleUri(YuzuApplication.appContext, Uri.parse(game.path))
                        ?.exists()
                if (gameExists == true) {
                    deserializedGames.add(game)
                }
            }
            setGames(deserializedGames.toList())
        }
        reloadGames(false)
    }

    fun setGames(games: List<Game>) {
        val sortedList = games.sortedWith(
            compareBy(
                { it.title.lowercase(Locale.getDefault()) },
                { it.path }
            )
        )

        _games.postValue(sortedList)
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
                setGames(GameHelper.getGames())
                _isReloading.postValue(false)

                if (directoryChanged) {
                    setShouldSwapData(true)
                }
            }
        }
    }
}
