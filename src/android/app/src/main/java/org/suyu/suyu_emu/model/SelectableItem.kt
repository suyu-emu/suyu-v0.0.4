// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.model

interface SelectableItem {
    var selected: Boolean
    fun onSelectionStateChanged(selected: Boolean)
}
