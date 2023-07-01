// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.app.ActivityManager
import android.content.Context
import org.yuzu.yuzu_emu.R
import java.util.Locale

class MemoryUtil(val context: Context) {

    private val Long.floatForm: String
        get() = String.format(Locale.ROOT, "%.2f", this.toDouble())

    private fun bytesToSizeUnit(size: Long): String {
        return when {
            size < Kb -> "${size.floatForm} ${context.getString(R.string.memory_byte)}"
            size < Mb -> "${(size / Kb).floatForm} ${context.getString(R.string.memory_kilobyte)}"
            size < Gb -> "${(size / Mb).floatForm} ${context.getString(R.string.memory_megabyte)}"
            size < Tb -> "${(size / Gb).floatForm} ${context.getString(R.string.memory_gigabyte)}"
            size < Pb -> "${(size / Tb).floatForm} ${context.getString(R.string.memory_terabyte)}"
            size < Eb -> "${(size / Pb).floatForm} ${context.getString(R.string.memory_petabyte)}"
            else -> "${(size / Eb).floatForm} ${context.getString(R.string.memory_exabyte)}"
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
