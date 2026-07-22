// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.model

import android.os.Parcelable
import kotlinx.parcelize.Parcelize

@Parcelize
data class UserProfile(
    val uuid: String,
    val username: String,
    val imagePath: String = ""
) : Parcelable

object ProfileUtils {
    fun generateRandomUUID(): String {
        val uuid = java.util.UUID.randomUUID()
        return uuid.toString().replace("-", "")
    }
}
