// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.overlay.model

import androidx.annotation.IntegerRes

data class OverlayControlDefault(
    val buttonId: String,
    @IntegerRes val landscapePositionResource: Pair<Int, Int>,
    @IntegerRes val portraitPositionResource: Pair<Int, Int>,
    @IntegerRes val foldablePositionResource: Pair<Int, Int>
)
