// SPDX-FileCopyrightText: 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.features.input

import android.content.Context
import android.os.Build
import android.os.CombinedVibration
import android.os.VibrationEffect
import android.os.Vibrator
import android.os.VibratorManager
import android.view.InputDevice
import androidx.annotation.Keep
import androidx.annotation.RequiresApi
import org.suyu.suyu_emu.SuyuApplication

@Keep
@Suppress("DEPRECATION")
interface SuyuVibrator {
    fun supportsVibration(): Boolean

    fun vibrate(intensity: Float)

    companion object {
        fun getControllerVibrator(device: InputDevice): SuyuVibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                SuyuVibratorManager(device.vibratorManager)
            } else {
                SuyuVibratorManagerCompat(device.vibrator)
            }

        fun getSystemVibrator(): SuyuVibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager = SuyuApplication.appContext
                    .getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
                SuyuVibratorManager(vibratorManager)
            } else {
                val vibrator = SuyuApplication.appContext
                    .getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
                SuyuVibratorManagerCompat(vibrator)
            }

        fun getVibrationEffect(intensity: Float): VibrationEffect? {
            if (intensity > 0f) {
                return VibrationEffect.createOneShot(
                    50,
                    (255.0 * intensity).toInt().coerceIn(1, 255)
                )
            }
            return null
        }
    }
}

@RequiresApi(Build.VERSION_CODES.S)
class SuyuVibratorManager(private val vibratorManager: VibratorManager) : SuyuVibrator {
    override fun supportsVibration(): Boolean {
        return vibratorManager.vibratorIds.isNotEmpty()
    }

    override fun vibrate(intensity: Float) {
        val vibration = SuyuVibrator.getVibrationEffect(intensity) ?: return
        vibratorManager.vibrate(CombinedVibration.createParallel(vibration))
    }
}

class SuyuVibratorManagerCompat(private val vibrator: Vibrator) : SuyuVibrator {
    override fun supportsVibration(): Boolean {
        return vibrator.hasVibrator()
    }

    override fun vibrate(intensity: Float) {
        val vibration = SuyuVibrator.getVibrationEffect(intensity) ?: return
        vibrator.vibrate(vibration)
    }
}
