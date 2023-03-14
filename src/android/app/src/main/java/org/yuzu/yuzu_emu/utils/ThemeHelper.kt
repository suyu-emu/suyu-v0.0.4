// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.Activity
import android.content.res.Configuration
import android.graphics.Color
import androidx.annotation.ColorInt
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.view.WindowCompat
import com.google.android.material.color.MaterialColors
import org.yuzu.yuzu_emu.R
import kotlin.math.roundToInt

object ThemeHelper {
    private const val NAV_BAR_ALPHA = 0.9f

    @JvmStatic
    fun setTheme(activity: AppCompatActivity) {
        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        val isLightMode =
            (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_NO
        windowController.isAppearanceLightStatusBars = isLightMode
        windowController.isAppearanceLightNavigationBars = isLightMode

        activity.window.statusBarColor = ContextCompat.getColor(activity, android.R.color.transparent)

        val navigationBarColor =
            MaterialColors.getColor(activity.window.decorView, R.attr.colorSurface)
        setNavigationBarColor(activity, navigationBarColor)
    }

    @JvmStatic
    fun setNavigationBarColor(activity: Activity, @ColorInt color: Int) {
        val gestureType = InsetsHelper.getSystemGestureType(activity.applicationContext)
        val orientation = activity.resources.configuration.orientation

        if ((gestureType == InsetsHelper.THREE_BUTTON_NAVIGATION ||
                    gestureType == InsetsHelper.TWO_BUTTON_NAVIGATION) &&
            orientation == Configuration.ORIENTATION_LANDSCAPE
        ) {
            activity.window.navigationBarColor = color
        } else if (gestureType == InsetsHelper.THREE_BUTTON_NAVIGATION ||
            gestureType == InsetsHelper.TWO_BUTTON_NAVIGATION
        ) {
            activity.window.navigationBarColor = getColorWithOpacity(color, NAV_BAR_ALPHA)
        } else {
            activity.window.navigationBarColor = ContextCompat.getColor(
                activity.applicationContext,
                android.R.color.transparent
            )
        }
    }

    @ColorInt
    private fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(), Color.red(color),
            Color.green(color), Color.blue(color)
        )
    }
}
