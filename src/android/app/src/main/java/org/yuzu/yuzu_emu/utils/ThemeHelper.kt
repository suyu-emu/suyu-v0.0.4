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
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.ui.main.ThemeProvider
import kotlin.math.roundToInt

object ThemeHelper {
    const val SYSTEM_BAR_ALPHA = 0.9f

    private const val DEFAULT = 0
    private const val MATERIAL_YOU = 1

    @JvmStatic
    fun setTheme(activity: AppCompatActivity) {
        val preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        when (preferences.getInt(Settings.PREF_THEME, 0)) {
            DEFAULT -> activity.setTheme(R.style.Theme_Yuzu_Main)
            MATERIAL_YOU -> activity.setTheme(R.style.Theme_Yuzu_Main_MaterialYou)
        }

        val windowController = WindowCompat.getInsetsController(
            activity.window,
            activity.window.decorView
        )
        val isLightMode =
            (activity.resources.configuration.uiMode and Configuration.UI_MODE_NIGHT_MASK) == Configuration.UI_MODE_NIGHT_NO
        windowController.isAppearanceLightStatusBars = isLightMode
        windowController.isAppearanceLightNavigationBars = isLightMode

        activity.window.statusBarColor = ContextCompat.getColor(activity, android.R.color.transparent)
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
            activity.window.navigationBarColor = getColorWithOpacity(color, SYSTEM_BAR_ALPHA)
        } else {
            activity.window.navigationBarColor = ContextCompat.getColor(
                activity.applicationContext,
                android.R.color.transparent
            )
        }
    }

    @ColorInt
    fun getColorWithOpacity(@ColorInt color: Int, alphaFactor: Float): Int {
        return Color.argb(
            (alphaFactor * Color.alpha(color)).roundToInt(), Color.red(color),
            Color.green(color), Color.blue(color)
        )
    }

    fun setCorrectTheme(activity: AppCompatActivity) {
        val currentTheme = (activity as ThemeProvider).themeId
        setTheme(activity)
        if (currentTheme != (activity as ThemeProvider).themeId) {
            activity.recreate()
        }
    }
}
