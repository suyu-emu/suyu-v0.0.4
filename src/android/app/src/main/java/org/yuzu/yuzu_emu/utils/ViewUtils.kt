// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.view.View

object ViewUtils {
    fun showView(view: View, length: Long = 300) {
        view.apply {
            alpha = 0f
            visibility = View.VISIBLE
            isClickable = true
        }.animate().apply {
            duration = length
            alpha(1f)
        }.start()
    }

    fun hideView(view: View, length: Long = 300) {
        if (view.visibility == View.INVISIBLE) {
            return
        }

        view.apply {
            alpha = 1f
            isClickable = false
        }.animate().apply {
            duration = length
            alpha(0f)
        }.withEndAction {
            view.visibility = View.INVISIBLE
        }.start()
    }
}
