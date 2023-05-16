// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.util.Log
import org.yuzu.yuzu_emu.BuildConfig

/**
 * Contains methods that call through to [android.util.Log], but
 * with the same TAG automatically provided. Also no-ops VERBOSE and DEBUG log
 * levels in release builds.
 */
object Log {
    private const val TAG = "Yuzu Frontend"

    fun verbose(message: String) {
        if (BuildConfig.DEBUG) {
            Log.v(TAG, message)
        }
    }

    fun debug(message: String) {
        if (BuildConfig.DEBUG) {
            Log.d(TAG, message)
        }
    }

    fun info(message: String) {
        Log.i(TAG, message)
    }

    fun warning(message: String) {
        Log.w(TAG, message)
    }

    fun error(message: String) {
        Log.e(TAG, message)
    }
}
