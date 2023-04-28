// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

data class HomeSetting(
    val titleId: Int,
    val descriptionId: Int,
    val iconId: Int,
    val onClick: () -> Unit
)
