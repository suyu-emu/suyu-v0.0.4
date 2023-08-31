// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.net.Uri
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.ViewModel
import androidx.lifecycle.viewModelScope
import androidx.preference.PreferenceManager
import java.util.Locale
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import kotlinx.serialization.ExperimentalSerializationApi
import kotlinx.serialization.MissingFieldException
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.json.Json
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.GameHelper

@OptIn(ExperimentalSerializationApi::class)
class GamesViewModel : ViewModel() {
    val games: StateFlow<List<Game>> get() = _games
    private val _games = MutableStateFlow(emptyList<Game>())

    val searchedGames: StateFlow<List<Game>> get() = _searchedGames
    private val _searchedGames = MutableStateFlow(emptyList<Game>())

    val isReloading: StateFlow<Boolean> get() = _isReloading
    private val _isReloading = MutableStateFlow(false)

    val shouldSwapData: StateFlow<Boolean> get() = _shouldSwapData
    private val _shouldSwapData = MutableStateFlow(false)

    val shouldScrollToTop: StateFlow<Boolean> get() = _shouldScrollToTop
    private val _shouldScrollToTop = MutableStateFlow(false)

    val searchFocused: StateFlow<Boolean> get() = _searchFocused
    private val _searchFocused = MutableStateFlow(false)

    init {
        // Ensure keys are loaded so that ROM metadata can be decrypted.
        NativeLibrary.reloadKeys()

        // Retrieve list of cached games
        val storedGames = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
            .getStringSet(GameHelper.KEY_GAMES, emptySet())
        if (storedGames!!.isNotEmpty()) {
            val deserializedGames = mutableSetOf<Game>()
            storedGames.forEach {
                val game: Game
                try {
                    game = Json.decodeFromString(it)
                } catch (e: MissingFieldException) {
                    return@forEach
                }

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

        _games.value = sortedList
    }

    fun setSearchedGames(games: List<Game>) {
        _searchedGames.value = games
    }

    fun setShouldSwapData(shouldSwap: Boolean) {
        _shouldSwapData.value = shouldSwap
    }

    fun setShouldScrollToTop(shouldScroll: Boolean) {
        _shouldScrollToTop.value = shouldScroll
    }

    fun setSearchFocused(searchFocused: Boolean) {
        _searchFocused.value = searchFocused
    }

    fun reloadGames(directoryChanged: Boolean) {
        if (isReloading.value) {
            return
        }
        _isReloading.value = true

        viewModelScope.launch {
            withContext(Dispatchers.IO) {
                NativeLibrary.resetRomMetadata()
                setGames(GameHelper.getGames())
                _isReloading.value = false

                if (directoryChanged) {
                    setShouldSwapData(true)
                }
            }
        }
    }
}
