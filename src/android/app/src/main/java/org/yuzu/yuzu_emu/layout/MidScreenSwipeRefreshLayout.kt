// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.ui

import org.yuzu.yuzu_emu.R
import android.content.Context
import android.util.AttributeSet
import android.view.MotionEvent
import androidx.swiperefreshlayout.widget.SwipeRefreshLayout

class MidScreenSwipeRefreshLayout @JvmOverloads constructor(
    context: Context,
    attrs: AttributeSet? = null
) : SwipeRefreshLayout(context, attrs) {

    private var startX = 0f
    private var allowRefresh = false

    override fun onInterceptTouchEvent(ev: MotionEvent): Boolean {
        when (ev.actionMasked) {
            MotionEvent.ACTION_DOWN -> {
                startX = ev.x
                val width = width
                val center_fraction = resources.getFraction(
                    R.fraction.carousel_midscreenswipe_width_fraction,
                    1,
                    1
                ).coerceIn(0f, 1f)
                val leftBound = ((1 - center_fraction) / 2) * width
                val rightBound = leftBound + (width * center_fraction)
                allowRefresh = startX >= leftBound && startX <= rightBound
            }
        }
        return if (allowRefresh) super.onInterceptTouchEvent(ev) else false
    }
}
