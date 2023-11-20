// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.os.Parcelable
import java.util.HashSet
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable

@Parcelize
@Serializable
class Game(
    val title: String = "",
    val path: String,
    val programId: String = "",
    val developer: String = "",
    val version: String = "",
    val isHomebrew: Boolean = false
) : Parcelable {
    val keyAddedToLibraryTime get() = "${path}_AddedToLibraryTime"
    val keyLastPlayedTime get() = "${path}_LastPlayed"

    override fun equals(other: Any?): Boolean {
        if (other !is Game) {
            return false
        }

        return hashCode() == other.hashCode()
    }

    override fun hashCode(): Int {
        var result = title.hashCode()
        result = 31 * result + path.hashCode()
        result = 31 * result + programId.hashCode()
        result = 31 * result + developer.hashCode()
        result = 31 * result + isHomebrew.hashCode()
        return result
    }

    companion object {
        val extensions: Set<String> = HashSet(
            listOf("xci", "nsp", "nca", "nro")
        )
    }
}
