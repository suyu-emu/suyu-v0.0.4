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
import org.suyu.suyu_emu.suyuApplication

@Keep
@Suppress("DEPRECATION")
interface suyuVibrator {
    fun supportsVibration(): Boolean

    fun vibrate(intensity: Float)

    companion object {
        fun getControllerVibrator(device: InputDevice): suyuVibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                suyuVibratorManager(device.vibratorManager)
            } else {
                suyuVibratorManagerCompat(device.vibrator)
            }

        fun getSystemVibrator(): suyuVibrator =
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                val vibratorManager = suyuApplication.appContext
                    .getSystemService(Context.VIBRATOR_MANAGER_SERVICE) as VibratorManager
                suyuVibratorManager(vibratorManager)
            } else {
                val vibrator = suyuApplication.appContext
                    .getSystemService(Context.VIBRATOR_SERVICE) as Vibrator
                suyuVibratorManagerCompat(vibrator)
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
class suyuVibratorManager(private val vibratorManager: VibratorManager) : suyuVibrator {
    override fun supportsVibration(): Boolean {
        return vibratorManager.vibratorIds.isNotEmpty()
    }

    override fun vibrate(intensity: Float) {
        val vibration = suyuVibrator.getVibrationEffect(intensity) ?: return
        vibratorManager.vibrate(CombinedVibration.createParallel(vibration))
    }
}

class suyuVibratorManagerCompat(private val vibrator: Vibrator) : suyuVibrator {
    override fun supportsVibration(): Boolean {
        return vibrator.hasVibrator()
    }

    override fun vibrate(intensity: Float) {
        val vibration = suyuVibrator.getVibrationEffect(intensity) ?: return
        vibrator.vibrate(vibration)
    }
}
