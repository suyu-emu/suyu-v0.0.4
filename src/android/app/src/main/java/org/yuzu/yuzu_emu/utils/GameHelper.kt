// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.SharedPreferences
import android.net.Uri
import androidx.preference.PreferenceManager
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.MinimalDocumentFile

object GameHelper {
    const val KEY_GAME_PATH = "game_path"
    const val KEY_GAMES = "Games"

    private lateinit var preferences: SharedPreferences

    fun getGames(): List<Game> {
        val games = mutableListOf<Game>()
        val context = YuzuApplication.appContext
        val gamesDir =
            PreferenceManager.getDefaultSharedPreferences(context).getString(KEY_GAME_PATH, "")
        val gamesUri = Uri.parse(gamesDir)
        preferences = PreferenceManager.getDefaultSharedPreferences(context)

        // Ensure keys are loaded so that ROM metadata can be decrypted.
        NativeLibrary.reloadKeys()

        addGamesRecursive(games, FileUtil.listFiles(context, gamesUri), 3)

        // Cache list of games found on disk
        val serializedGames = mutableSetOf<String>()
        games.forEach {
            serializedGames.add(Json.encodeToString(it))
        }
        preferences.edit()
            .remove(KEY_GAMES)
            .putStringSet(KEY_GAMES, serializedGames)
            .apply()

        return games.toList()
    }

    private fun addGamesRecursive(
        games: MutableList<Game>,
        files: Array<MinimalDocumentFile>,
        depth: Int
    ) {
        if (depth <= 0) {
            return
        }

        files.forEach {
            if (it.isDirectory) {
                addGamesRecursive(
                    games,
                    FileUtil.listFiles(YuzuApplication.appContext, it.uri),
                    depth - 1
                )
            } else {
                if (Game.extensions.contains(FileUtil.getExtension(it.uri))) {
                    games.add(getGame(it.uri, true))
                }
            }
        }
    }

    fun getGame(uri: Uri, addedToLibrary: Boolean): Game {
        val filePath = uri.toString()
        var name = NativeLibrary.getTitle(filePath)

        // If the game's title field is empty, use the filename.
        if (name.isEmpty()) {
            name = FileUtil.getFilename(uri)
        }
        var gameId = NativeLibrary.getGameId(filePath)

        // If the game's ID field is empty, use the filename without extension.
        if (gameId.isEmpty()) {
            gameId = name.substring(0, name.lastIndexOf("."))
        }

        val newGame = Game(
            name,
            NativeLibrary.getDescription(filePath).replace("\n", " "),
            NativeLibrary.getRegions(filePath),
            filePath,
            gameId,
            NativeLibrary.getCompany(filePath),
            NativeLibrary.isHomebrew(filePath)
        )

        if (addedToLibrary) {
            val addedTime = preferences.getLong(newGame.keyAddedToLibraryTime, 0L)
            if (addedTime == 0L) {
                preferences.edit()
                    .putLong(newGame.keyAddedToLibraryTime, System.currentTimeMillis())
                    .apply()
            }
        }

        return newGame
    }
}
