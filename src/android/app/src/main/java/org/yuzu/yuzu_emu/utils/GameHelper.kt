// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.SharedPreferences
import android.net.Uri
import androidx.preference.PreferenceManager
import kotlinx.serialization.decodeFromString
import kotlinx.serialization.encodeToString
import kotlinx.serialization.json.Json
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.model.Game
import java.util.*

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

        val children = FileUtil.listFiles(context, gamesUri)
        for (file in children) {
            if (!file.isDirectory) {
                val filename = file.uri.toString()
                val extensionStart = filename.lastIndexOf('.')
                if (extensionStart > 0) {
                    val fileExtension = filename.substring(extensionStart)

                    // Check that the file has an extension we care about before trying to read out of it.
                    if (Game.extensions.contains(fileExtension.lowercase(Locale.getDefault()))) {
                        games.add(getGame(filename))
                    }
                }
            }
        }

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

    private fun getGame(filePath: String): Game {
        var name = NativeLibrary.getTitle(filePath)

        // If the game's title field is empty, use the filename.
        if (name.isEmpty()) {
            name = filePath.substring(filePath.lastIndexOf("/") + 1)
        }
        var gameId = NativeLibrary.getGameId(filePath)

        // If the game's ID field is empty, use the filename without extension.
        if (gameId.isEmpty()) {
            gameId = filePath.substring(
                filePath.lastIndexOf("/") + 1,
                filePath.lastIndexOf(".")
            )
        }

        val newGame = Game(
            name,
            NativeLibrary.getDescription(filePath).replace("\n", " "),
            NativeLibrary.getRegions(filePath),
            filePath,
            gameId,
            NativeLibrary.getCompany(filePath)
        )

        val addedTime = preferences.getLong(newGame.keyAddedToLibraryTime, 0L)
        if (addedTime == 0L) {
            preferences.edit()
                .putLong(newGame.keyAddedToLibraryTime, System.currentTimeMillis())
                .apply()
        }

        return newGame
    }
}
