package org.yuzu.yuzu_emu.model

import androidx.lifecycle.LiveData
import androidx.lifecycle.MutableLiveData
import androidx.lifecycle.ViewModel

class GamesViewModel : ViewModel() {
    private val _games = MutableLiveData<ArrayList<Game>>()
    val games: LiveData<ArrayList<Game>> get() = _games

    init {
        _games.value = ArrayList()
    }

    fun setGames(games: ArrayList<Game>) {
        _games.value = games
    }
}
