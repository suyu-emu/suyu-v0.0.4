// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import org.yuzu.yuzu_emu.utils.DirectoryInitialization.DirectoryInitializationState

class DirectoryStateReceiver(var callback: (DirectoryInitializationState) -> Unit) :
    BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val state = intent
            .getSerializableExtra(DirectoryInitialization.EXTRA_STATE) as DirectoryInitializationState
        callback.invoke(state)
    }
}
