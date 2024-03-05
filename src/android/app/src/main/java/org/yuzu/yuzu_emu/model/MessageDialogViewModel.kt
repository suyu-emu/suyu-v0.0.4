// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.model

import androidx.lifecycle.ViewModel

class MessageDialogViewModel : ViewModel() {
    var positiveAction: (() -> Unit)? = null
    var negativeAction: (() -> Unit)? = null

    fun clear() {
        positiveAction = null
        negativeAction = null
    }
}
