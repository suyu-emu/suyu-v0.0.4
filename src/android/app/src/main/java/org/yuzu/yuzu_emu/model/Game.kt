// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.content.ContentValues
import android.database.Cursor
import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import java.nio.file.Paths
import java.util.HashSet

@Parcelize
class Game(
    val title: String,
    val description: String,
    val regions: String,
    val path: String,
    val gameId: String,
    val company: String
) : Parcelable {
    companion object {
        val extensions: Set<String> = HashSet(
            listOf(".xci", ".nsp", ".nca", ".nro")
        )

        @JvmStatic
        fun asContentValues(
            title: String?,
            description: String?,
            regions: String?,
            path: String?,
            gameId: String,
            company: String?
        ): ContentValues {
            var realGameId = gameId
            val values = ContentValues()
            if (realGameId.isEmpty()) {
                // Homebrew, etc. may not have a game ID, use filename as a unique identifier
                realGameId = Paths.get(path).fileName.toString()
            }
            values.put(GameDatabase.KEY_GAME_TITLE, title)
            values.put(GameDatabase.KEY_GAME_DESCRIPTION, description)
            values.put(GameDatabase.KEY_GAME_REGIONS, regions)
            values.put(GameDatabase.KEY_GAME_PATH, path)
            values.put(GameDatabase.KEY_GAME_ID, realGameId)
            values.put(GameDatabase.KEY_GAME_COMPANY, company)
            return values
        }

        fun fromCursor(cursor: Cursor): Game {
            return Game(
                cursor.getString(GameDatabase.GAME_COLUMN_TITLE),
                cursor.getString(GameDatabase.GAME_COLUMN_DESCRIPTION),
                cursor.getString(GameDatabase.GAME_COLUMN_REGIONS),
                cursor.getString(GameDatabase.GAME_COLUMN_PATH),
                cursor.getString(GameDatabase.GAME_COLUMN_GAME_ID),
                cursor.getString(GameDatabase.GAME_COLUMN_CAPTION)
            )
        }
    }
}
