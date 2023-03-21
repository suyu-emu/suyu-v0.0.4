package org.yuzu.yuzu_emu.utils

import android.annotation.SuppressLint
import android.app.Activity
import android.content.Context
import android.graphics.Rect
import android.view.ViewGroup.MarginLayoutParams
import androidx.core.graphics.Insets
import com.google.android.material.appbar.AppBarLayout

object InsetsHelper {
    const val THREE_BUTTON_NAVIGATION = 0
    const val TWO_BUTTON_NAVIGATION = 1
    const val GESTURE_NAVIGATION = 2

    fun insetAppBar(insets: Insets, appBarLayout: AppBarLayout) {
        val mlpAppBar = appBarLayout.layoutParams as MarginLayoutParams
        mlpAppBar.leftMargin = insets.left
        mlpAppBar.rightMargin = insets.right
        appBarLayout.layoutParams = mlpAppBar
    }

    @SuppressLint("DiscouragedApi")
    fun getSystemGestureType(context: Context): Int {
        val resources = context.resources
        val resourceId =
            resources.getIdentifier("config_navBarInteractionMode", "integer", "android")
        return if (resourceId != 0) {
            resources.getInteger(resourceId)
        } else 0
    }

    fun getBottomPaddingRequired(activity: Activity): Int {
        val visibleFrame = Rect()
        activity.window.decorView.getWindowVisibleDisplayFrame(visibleFrame)
        return visibleFrame.bottom - visibleFrame.top - activity.resources.displayMetrics.heightPixels
    }
}
