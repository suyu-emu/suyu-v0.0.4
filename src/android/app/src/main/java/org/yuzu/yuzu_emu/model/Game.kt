// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.os.Parcelable
import kotlinx.parcelize.Parcelize
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
    }
}
