// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.model

import androidx.lifecycle.ViewModel

class MessageDialogViewModel : ViewModel() {
    var dismissAction: () -> Unit = {}

    fun clear() {
        dismissAction = {}
    }
}
