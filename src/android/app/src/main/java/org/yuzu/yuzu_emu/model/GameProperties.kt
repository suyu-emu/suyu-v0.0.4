// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.annotation.DrawableRes
import androidx.annotation.StringRes
import kotlinx.coroutines.flow.StateFlow

interface GameProperty {
    @get:StringRes
    val titleId: Int

    @get:StringRes
    val descriptionId: Int

    @get:DrawableRes
    val iconId: Int
}

data class SubmenuProperty(
    override val titleId: Int,
    override val descriptionId: Int,
    override val iconId: Int,
    val details: (() -> String)? = null,
    val detailsFlow: StateFlow<String>? = null,
    val action: () -> Unit,
    val secondaryActions: List<SubMenuPropertySecondaryAction>? = null
) : GameProperty

data class SubMenuPropertySecondaryAction(
    val isShown : Boolean,
    val descriptionId: Int,
    val iconId: Int,
    val action: () -> Unit
)

data class InstallableProperty(
    override val titleId: Int,
    override val descriptionId: Int,
    override val iconId: Int,
    val install: (() -> Unit)? = null,
    val export: (() -> Unit)? = null
) : GameProperty
