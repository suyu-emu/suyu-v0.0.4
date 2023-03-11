// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.util.AttributeSet
import android.widget.FrameLayout

/**
 * FrameLayout subclass with few Properties added to simplify animations.
 * Don't remove the methods appearing as unused, in order not to break the menu animations
 */
class SettingsFrameLayout : FrameLayout {
    private val mVisibleness = 1.0f

    constructor(context: Context?) : super(context!!)
    constructor(context: Context?, attrs: AttributeSet?) : super(context!!, attrs)

    constructor(
        context: Context?,
        attrs: AttributeSet?,
        defStyleAttr: Int
    ) : super(context!!, attrs, defStyleAttr)

    constructor(
        context: Context?,
        attrs: AttributeSet?,
        defStyleAttr: Int,
        defStyleRes: Int
    ) : super(context!!, attrs, defStyleAttr, defStyleRes)

    var yFraction: Float
        get() = y / height
        set(yFraction) {
            val height = height
            y = (if (height > 0) yFraction * height else -9999) as Float
        }
    var visibleness: Float
        get() = mVisibleness
        set(visibleness) {
            scaleX = visibleness
            scaleY = visibleness
            alpha = visibleness
        }
}
