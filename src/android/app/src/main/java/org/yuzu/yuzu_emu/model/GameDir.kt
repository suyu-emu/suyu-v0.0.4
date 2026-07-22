// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import android.os.Parcelable
import kotlinx.parcelize.Parcelize

@Parcelize
data class GameDir(
    val uriString: String,
    var deepScan: Boolean,
    val type: DirectoryType = DirectoryType.GAME
) : Parcelable {
    // Needed for JNI backward compatability
    constructor(uriString: String, deepScan: Boolean) : this(uriString, deepScan, DirectoryType.GAME)
}

enum class DirectoryType {
    GAME,
    EXTERNAL_CONTENT
}
