package org.yuzu.yuzu_emu.ui.platform

import android.database.Cursor

/**
 * Abstraction for a screen representing a single platform's games.
 */
interface PlatformGamesView {
    /**
     * Tell the view to refresh its contents.
     */
    fun refresh()

    /**
     * To be called when an asynchronous database read completes. Passes the
     * result, in this case a [Cursor], to the view.
     *
     * @param games A Cursor containing the games read from the database.
     */
    fun showGames(games: Cursor)
}
