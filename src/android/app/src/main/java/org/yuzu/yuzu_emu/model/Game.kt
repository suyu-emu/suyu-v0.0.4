// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import java.util.HashSet

@Parcelize
@Serializable
class Game(
    val title: String,
    val description: String,
    val regions: String,
    val path: String,
    val gameId: String,
    val company: String
) : Parcelable {
    val keyAddedToLibraryTime get() = "${gameId}_AddedToLibraryTime"
    val keyLastPlayedTime get() = "${gameId}_LastPlayed"

    override fun equals(other: Any?): Boolean {
        if (other !is Game)
            return false

        return title == other.title
                && description == other.description
                && regions == other.regions
                && path == other.path
                && gameId == other.gameId
                && company == other.company
    }

    companion object {
        val extensions: Set<String> = HashSet(
            listOf(".xci", ".nsp", ".nca", ".nro")
        )
    }
}
