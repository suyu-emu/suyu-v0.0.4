// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.Activity
import android.content.Context
import android.view.Window
import androidx.core.content.edit
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.features.settings.model.Settings

object FullscreenHelper {
    fun isFullscreenEnabled(context: Context): Boolean {
        return PreferenceManager.getDefaultSharedPreferences(context).getBoolean(
            Settings.PREF_APP_FULLSCREEN,
            Settings.APP_FULLSCREEN_DEFAULT
        )
    }

    fun setFullscreenEnabled(context: Context, enabled: Boolean) {
        PreferenceManager.getDefaultSharedPreferences(context).edit {
            putBoolean(Settings.PREF_APP_FULLSCREEN, enabled)
        }
    }

    fun shouldHideSystemBars(activity: Activity): Boolean {
        val rootInsets = ViewCompat.getRootWindowInsets(activity.window.decorView)
        val barsCurrentlyHidden =
            rootInsets?.isVisible(WindowInsetsCompat.Type.systemBars())?.not() ?: false
        return isFullscreenEnabled(activity) || barsCurrentlyHidden
    }

    fun applyToWindow(window: Window, hideSystemBars: Boolean) {
        val controller = WindowInsetsControllerCompat(window, window.decorView)

        if (hideSystemBars) {
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        } else {
            controller.show(WindowInsetsCompat.Type.systemBars())
        }
    }

    fun applyToActivity(activity: Activity) {
        applyToWindow(activity.window, isFullscreenEnabled(activity))
    }
}
