// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.BatteryManager
import android.os.Build

object PowerStateUtils {

    @JvmStatic
    fun getBatteryInfo(context: Context?): IntArray {
        if (context == null) {
            return intArrayOf(0, 0, 0) // Percentage, IsCharging, HasBattery
        }

        val results = intArrayOf(100, 0, 1)
        val iFilter = IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        val batteryStatusIntent: Intent? = context.registerReceiver(null, iFilter)

        if (batteryStatusIntent != null) {
            val present = batteryStatusIntent.getBooleanExtra(BatteryManager.EXTRA_PRESENT, true)
            if (!present) {
                results[2] = 0; results[0] = 0; results[1] = 0; return results
            }
            results[2] = 1
            val level = batteryStatusIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
            val scale = batteryStatusIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
            if (level != -1 && scale != -1 && scale != 0) {
                results[0] = (level.toFloat() / scale.toFloat() * 100.0f).toInt()
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                val bm = context.getSystemService(Context.BATTERY_SERVICE) as BatteryManager?
                results[0] = bm?.getIntProperty(BatteryManager.BATTERY_PROPERTY_CAPACITY) ?: 100
            }
            val status = batteryStatusIntent.getIntExtra(BatteryManager.EXTRA_STATUS, -1)
            results[1] = if (status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL) 1 else 0
        }

        return results
    }
}
