// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

data class SetupPage(
    val iconId: Int,
    val titleId: Int,
    val descriptionId: Int,
    val pageButtons: List<PageButton>? = null,
    val pageSteps: () -> PageState = { PageState.COMPLETE },

    )

data class PageButton(
    val iconId: Int,
    val titleId: Int,
    val descriptionId: Int,
    val buttonAction: (callback: SetupCallback) -> Unit,
    val buttonState: () -> ButtonState = { ButtonState.BUTTON_ACTION_UNDEFINED },
    val isUnskippable: Boolean = false,
    val hasWarning: Boolean = false,
    val warningTitleId: Int = 0,
    val warningDescriptionId: Int = 0,
    val warningHelpLinkId: Int = 0
)

interface SetupCallback {
    fun onStepCompleted(pageButtonId: Int, pageFullyCompleted: Boolean)
}

enum class PageState {
    COMPLETE,
    INCOMPLETE,
    UNDEFINED
}

enum class ButtonState {
    BUTTON_ACTION_COMPLETE,
    BUTTON_ACTION_INCOMPLETE,
    BUTTON_ACTION_UNDEFINED
}
