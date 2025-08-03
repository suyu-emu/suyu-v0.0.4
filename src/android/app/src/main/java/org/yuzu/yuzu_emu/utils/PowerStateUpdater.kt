// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.os.Handler
import android.os.Looper
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication

object PowerStateUpdater {

    private lateinit var handler: Handler
    private lateinit var runnable: Runnable
    private const val UPDATE_INTERVAL_MS = 1000L
    private var isStarted = false

    fun start() {
        if (isStarted) {
            return
        }

        val context = YuzuApplication.appContext

        handler = Handler(Looper.getMainLooper())
        runnable = Runnable {
            val info = PowerStateUtils.getBatteryInfo(context)
            NativeLibrary.updatePowerState(info[0], info[1] == 1, info[2] == 1)
            handler.postDelayed(runnable, UPDATE_INTERVAL_MS)
        }
        handler.post(runnable)
        isStarted = true
    }

    fun stop() {
        if (!isStarted) {
            return
        }
        if (::handler.isInitialized) {
            handler.removeCallbacks(runnable)
        }
        isStarted = false
    }
}
