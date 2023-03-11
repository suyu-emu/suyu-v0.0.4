// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.platform

import android.database.Cursor
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.Log
import rx.android.schedulers.AndroidSchedulers
import rx.schedulers.Schedulers

class PlatformGamesPresenter(private val view: PlatformGamesView) {
    fun onCreateView() {
        loadGames()
    }

    fun refresh() {
        Log.debug("[PlatformGamesPresenter] : Refreshing...")
        loadGames()
    }

    private fun loadGames() {
        Log.debug("[PlatformGamesPresenter] : Loading games...")
        val databaseHelper = YuzuApplication.databaseHelper
        databaseHelper!!.games
            .subscribeOn(Schedulers.io())
            .observeOn(AndroidSchedulers.mainThread())
            .subscribe { games: Cursor? ->
                Log.debug("[PlatformGamesPresenter] : Load finished, swapping cursor...")
                view.showGames(games!!)
            }
    }
}
