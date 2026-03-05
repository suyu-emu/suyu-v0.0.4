// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.fetcher

import android.graphics.Rect
import android.view.View
import androidx.recyclerview.widget.GridLayoutManager
import androidx.recyclerview.widget.RecyclerView

class SpacingItemDecoration(private val spacing: Int) : RecyclerView.ItemDecoration() {
    override fun getItemOffsets(
        outRect: Rect,
        view: View,
        parent: RecyclerView,
        state: RecyclerView.State
    ) {
        outRect.bottom = spacing

        val position = parent.getChildAdapterPosition(view)
        if (position == RecyclerView.NO_POSITION) return

        if (position == 0) {
            outRect.top = spacing
            return
        }

        // If the item is in the first row, but NOT in first column add top spacing as well
        val layoutManager = parent.layoutManager
        if (layoutManager is GridLayoutManager && layoutManager.spanSizeLookup.getSpanGroupIndex(position, layoutManager.spanCount) == 0) {
            outRect.top = spacing
            return
        }
    }
}
