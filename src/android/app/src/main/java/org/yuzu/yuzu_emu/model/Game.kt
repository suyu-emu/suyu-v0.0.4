// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.net.Uri
import android.os.Parcelable
import java.util.HashSet
import kotlinx.parcelize.Parcelize
import kotlinx.serialization.Serializable
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil
import java.time.LocalDateTime
import java.time.format.DateTimeFormatter

@Parcelize
@Serializable
class Game(
    val title: String = "",
    val path: String,
    val programId: String = "",
    val developer: String = "",
    var version: String = "",
    val isHomebrew: Boolean = false
) : Parcelable {
    val keyAddedToLibraryTime get() = "${path}_AddedToLibraryTime"
    val keyLastPlayedTime get() = "${path}_LastPlayed"

    val settingsName: String
        get() {
            val programIdLong = programId.toLong()
            return if (programIdLong == 0L) {
                FileUtil.getFilename(Uri.parse(path))
            } else {
                "0" + programIdLong.toString(16).uppercase()
            }
        }

    val programIdHex: String
        get() {
            val programIdLong = programId.toLong()
            return if (programIdLong == 0L) {
                "0"
            } else {
                "0" + programIdLong.toString(16).uppercase()
            }
        }

    val saveZipName: String
        get() = "$title ${YuzuApplication.appContext.getString(R.string.save_data).lowercase()} - ${
        LocalDateTime.now().format(DateTimeFormatter.ofPattern("yyyy-MM-dd HH:mm"))
        }.zip"

    val saveDir: String
        get() = DirectoryInitialization.userDirectory + "/nand" +
            NativeLibrary.getSavePath(programId)

    val addonDir: String
        get() = DirectoryInitialization.userDirectory + "/load/" + programIdHex + "/"

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
