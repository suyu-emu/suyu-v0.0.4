// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.ActivityManager
import android.content.Context
import java.util.Locale

class MemoryUtil(context: Context) {

    private val Long.floatForm: String
        get() = String.format(Locale.ROOT, "%.2f", this.toDouble())

    private fun bytesToSizeUnit(size: Long): String {
        return when {
            size < Kb -> size.floatForm + " byte"
            size < Mb -> (size / Kb).floatForm + " KB"
            size < Gb -> (size / Mb).floatForm + " MB"
            size < Tb -> (size / Gb).floatForm + " GB"
            size < Pb -> (size / Tb).floatForm + " TB"
            size < Eb -> (size / Pb).floatForm + " Pb"
            else -> (size / Eb).floatForm + " Eb"
        }
    }

    private val totalMemory =
        with(context.getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager) {
            val memInfo = ActivityManager.MemoryInfo()
            getMemoryInfo(memInfo)
            memInfo.totalMem
        }

    fun isLessThan(minimum: Int, size: Long): Boolean {
        return when (size) {
            Kb -> totalMemory < Mb && totalMemory < minimum
            Mb -> totalMemory < Gb && (totalMemory / Mb) < minimum
            Gb -> totalMemory < Tb && (totalMemory / Gb) < minimum
            Tb -> totalMemory < Pb && (totalMemory / Tb) < minimum
            Pb -> totalMemory < Eb && (totalMemory / Pb) < minimum
            Eb -> totalMemory / Eb < minimum
            else -> totalMemory < Kb && totalMemory < minimum
        }
    }

    fun getDeviceRAM(): String {
        return bytesToSizeUnit(totalMemory)
    }

    companion object {
        const val Kb: Long = 1024
        const val Mb = Kb * 1024
        const val Gb = Mb * 1024
        const val Tb = Gb * 1024
        const val Pb = Tb * 1024
        const val Eb = Pb * 1024
    }
}
