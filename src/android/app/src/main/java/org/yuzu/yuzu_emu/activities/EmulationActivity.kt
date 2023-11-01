// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.activities

import android.annotation.SuppressLint
import android.app.Activity
import android.app.PendingIntent
import android.app.PictureInPictureParams
import android.app.RemoteAction
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.res.Configuration
import android.graphics.Rect
import android.graphics.drawable.Icon
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Build
import android.os.Bundle
import android.util.Rational
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.View
import android.view.inputmethod.InputMethodManager
import android.widget.Toast
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.preference.PreferenceManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.ActivityEmulationBinding
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.features.settings.model.IntSetting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.model.EmulationViewModel
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.ForegroundService
import org.yuzu.yuzu_emu.utils.InputHandler
import org.yuzu.yuzu_emu.utils.Log
import org.yuzu.yuzu_emu.utils.MemoryUtil
import org.yuzu.yuzu_emu.utils.NfcReader
import org.yuzu.yuzu_emu.utils.ThemeHelper
import java.text.NumberFormat
import kotlin.math.roundToInt

class EmulationActivity : AppCompatActivity(), SensorEventListener {
    private lateinit var binding: ActivityEmulationBinding

    var isActivityRecreated = false
    private lateinit var nfcReader: NfcReader

    private val gyro = FloatArray(3)
    private val accel = FloatArray(3)
    private var motionTimestamp: Long = 0
    private var flipMotionOrientation: Boolean = false

    private var controllerIds = InputHandler.getGameControllerIds()

    private val actionPause = "ACTION_EMULATOR_PAUSE"
    private val actionPlay = "ACTION_EMULATOR_PLAY"
    private val actionMute = "ACTION_EMULATOR_MUTE"
    private val actionUnmute = "ACTION_EMULATOR_UNMUTE"

    private val emulationViewModel: EmulationViewModel by viewModels()

    override fun onDestroy() {
        stopForegroundService(this)
        emulationViewModel.clear()
        super.onDestroy()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        Log.gameLaunched = true
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityEmulationBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        navHostFragment.navController.setGraph(R.navigation.emulation_navigation, intent.extras)

        isActivityRecreated = savedInstanceState != null

        // Set these options now so that the SurfaceView the game renders into is the right size.
        enableFullscreenImmersive()

        window.decorView.setBackgroundColor(getColor(android.R.color.black))

        nfcReader = NfcReader(this)
        nfcReader.initialize()

        InputHandler.initialize()

        val preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        if (!preferences.getBoolean(Settings.PREF_MEMORY_WARNING_SHOWN, false)) {
            if (MemoryUtil.isLessThan(MemoryUtil.REQUIRED_MEMORY, MemoryUtil.Gb)) {
                Toast.makeText(
                    this,
                    getString(
                        R.string.device_memory_inadequate,
                        MemoryUtil.getDeviceRAM(),
                        getString(
                            R.string.memory_formatted,
                            NumberFormat.getInstance().format(MemoryUtil.REQUIRED_MEMORY),
                            getString(R.string.memory_gigabyte)
                        )
                    ),
                    Toast.LENGTH_LONG
                ).show()
                preferences.edit()
                    .putBoolean(Settings.PREF_MEMORY_WARNING_SHOWN, true)
                    .apply()
            }
        }

        // Start a foreground service to prevent the app from getting killed in the background
        val startIntent = Intent(this, ForegroundService::class.java)
        startForegroundService(startIntent)
    }

    override fun onKeyDown(keyCode: Int, event: KeyEvent): Boolean {
        if (event.action == KeyEvent.ACTION_DOWN) {
            if (keyCode == KeyEvent.KEYCODE_ENTER) {
                // Special case, we do not support multiline input, dismiss the keyboard.
                val overlayView: View =
                    this.findViewById(R.id.surface_input_overlay)
                val im =
                    overlayView.context.getSystemService(INPUT_METHOD_SERVICE) as InputMethodManager
                im.hideSoftInputFromWindow(overlayView.windowToken, 0)
            } else {
                val textChar = event.unicodeChar
                if (textChar == 0) {
                    // No text, button input.
                    NativeLibrary.submitInlineKeyboardInput(keyCode)
                } else {
                    // Text submitted.
                    NativeLibrary.submitInlineKeyboardText(textChar.toChar().toString())
                }
            }
        }
        return super.onKeyDown(keyCode, event)
    }

    override fun onResume() {
        super.onResume()
        nfcReader.startScanning()
        startMotionSensorListener()
        InputHandler.updateControllerIds()

        buildPictureInPictureParams()
    }

    override fun onPause() {
        super.onPause()
        nfcReader.stopScanning()
        stopMotionSensorListener()
    }

    override fun onUserLeaveHint() {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.S) {
            if (BooleanSetting.PICTURE_IN_PICTURE.boolean && !isInPictureInPictureMode) {
                val pictureInPictureParamsBuilder = PictureInPictureParams.Builder()
                    .getPictureInPictureActionsBuilder().getPictureInPictureAspectBuilder()
                enterPictureInPictureMode(pictureInPictureParamsBuilder.build())
            }
        }
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        nfcReader.onNewIntent(intent)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK &&
            event.source and InputDevice.SOURCE_GAMEPAD != InputDevice.SOURCE_GAMEPAD
        ) {
            return super.dispatchKeyEvent(event)
        }

        return InputHandler.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK &&
            event.source and InputDevice.SOURCE_GAMEPAD != InputDevice.SOURCE_GAMEPAD
        ) {
            return super.dispatchGenericMotionEvent(event)
        }

        // Don't attempt to do anything if we are disconnecting a device.
        if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
            return true
        }

        return InputHandler.dispatchGenericMotionEvent(event)
    }

    override fun onSensorChanged(event: SensorEvent) {
        val rotation = this.display?.rotation
        if (rotation == Surface.ROTATION_90) {
            flipMotionOrientation = true
        }
        if (rotation == Surface.ROTATION_270) {
            flipMotionOrientation = false
        }

        if (event.sensor.type == Sensor.TYPE_ACCELEROMETER) {
            if (flipMotionOrientation) {
                accel[0] = event.values[1] / SensorManager.GRAVITY_EARTH
                accel[1] = -event.values[0] / SensorManager.GRAVITY_EARTH
            } else {
                accel[0] = -event.values[1] / SensorManager.GRAVITY_EARTH
                accel[1] = event.values[0] / SensorManager.GRAVITY_EARTH
            }
            accel[2] = -event.values[2] / SensorManager.GRAVITY_EARTH
        }
        if (event.sensor.type == Sensor.TYPE_GYROSCOPE) {
            // Investigate why sensor value is off by 6x
            if (flipMotionOrientation) {
                gyro[0] = -event.values[1] / 6.0f
                gyro[1] = event.values[0] / 6.0f
            } else {
                gyro[0] = event.values[1] / 6.0f
                gyro[1] = -event.values[0] / 6.0f
            }
            gyro[2] = event.values[2] / 6.0f
        }

        // Only update state on accelerometer data
        if (event.sensor.type != Sensor.TYPE_ACCELEROMETER) {
            return
        }
        val deltaTimestamp = (event.timestamp - motionTimestamp) / 1000
        motionTimestamp = event.timestamp
        NativeLibrary.onGamePadMotionEvent(
            NativeLibrary.Player1Device,
            deltaTimestamp,
            gyro[0],
            gyro[1],
            gyro[2],
            accel[0],
            accel[1],
            accel[2]
        )
        NativeLibrary.onGamePadMotionEvent(
            NativeLibrary.ConsoleDevice,
            deltaTimestamp,
            gyro[0],
            gyro[1],
            gyro[2],
            accel[0],
            accel[1],
            accel[2]
        )
    }

    override fun onAccuracyChanged(sensor: Sensor, i: Int) {}

    private fun enableFullscreenImmersive() {
        WindowCompat.setDecorFitsSystemWindows(window, false)

        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
    }

    private fun PictureInPictureParams.Builder.getPictureInPictureAspectBuilder():
        PictureInPictureParams.Builder {
        val aspectRatio = when (IntSetting.RENDERER_ASPECT_RATIO.int) {
            0 -> Rational(16, 9)
            1 -> Rational(4, 3)
            2 -> Rational(21, 9)
            3 -> Rational(16, 10)
            else -> null // Best fit
        }
        return this.apply { aspectRatio?.let { setAspectRatio(it) } }
    }

    private fun PictureInPictureParams.Builder.getPictureInPictureActionsBuilder():
        PictureInPictureParams.Builder {
        val pictureInPictureActions: MutableList<RemoteAction> = mutableListOf()
        val pendingFlags = PendingIntent.FLAG_UPDATE_CURRENT or PendingIntent.FLAG_IMMUTABLE

        if (NativeLibrary.isPaused()) {
            val playIcon = Icon.createWithResource(this@EmulationActivity, R.drawable.ic_pip_play)
            val playPendingIntent = PendingIntent.getBroadcast(
                this@EmulationActivity,
                R.drawable.ic_pip_play,
                Intent(actionPlay),
                pendingFlags
            )
            val playRemoteAction = RemoteAction(
                playIcon,
                getString(R.string.play),
                getString(R.string.play),
                playPendingIntent
            )
            pictureInPictureActions.add(playRemoteAction)
        } else {
            val pauseIcon = Icon.createWithResource(this@EmulationActivity, R.drawable.ic_pip_pause)
            val pausePendingIntent = PendingIntent.getBroadcast(
                this@EmulationActivity,
                R.drawable.ic_pip_pause,
                Intent(actionPause),
                pendingFlags
            )
            val pauseRemoteAction = RemoteAction(
                pauseIcon,
                getString(R.string.pause),
                getString(R.string.pause),
                pausePendingIntent
            )
            pictureInPictureActions.add(pauseRemoteAction)
        }

        if (BooleanSetting.AUDIO_MUTED.boolean) {
            val unmuteIcon = Icon.createWithResource(
                this@EmulationActivity,
                R.drawable.ic_pip_unmute
            )
            val unmutePendingIntent = PendingIntent.getBroadcast(
                this@EmulationActivity,
                R.drawable.ic_pip_unmute,
                Intent(actionUnmute),
                pendingFlags
            )
            val unmuteRemoteAction = RemoteAction(
                unmuteIcon,
                getString(R.string.unmute),
                getString(R.string.unmute),
                unmutePendingIntent
            )
            pictureInPictureActions.add(unmuteRemoteAction)
        } else {
            val muteIcon = Icon.createWithResource(this@EmulationActivity, R.drawable.ic_pip_mute)
            val mutePendingIntent = PendingIntent.getBroadcast(
                this@EmulationActivity,
                R.drawable.ic_pip_mute,
                Intent(actionMute),
                pendingFlags
            )
            val muteRemoteAction = RemoteAction(
                muteIcon,
                getString(R.string.mute),
                getString(R.string.mute),
                mutePendingIntent
            )
            pictureInPictureActions.add(muteRemoteAction)
        }

        return this.apply { setActions(pictureInPictureActions) }
    }

    fun buildPictureInPictureParams() {
        val pictureInPictureParamsBuilder = PictureInPictureParams.Builder()
            .getPictureInPictureActionsBuilder().getPictureInPictureAspectBuilder()
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            pictureInPictureParamsBuilder.setAutoEnterEnabled(
                BooleanSetting.PICTURE_IN_PICTURE.boolean
            )
        }
        setPictureInPictureParams(pictureInPictureParamsBuilder.build())
    }

    private var pictureInPictureReceiver = object : BroadcastReceiver() {
        override fun onReceive(context: Context?, intent: Intent) {
            if (intent.action == actionPlay) {
                if (NativeLibrary.isPaused()) NativeLibrary.unpauseEmulation()
            } else if (intent.action == actionPause) {
                if (!NativeLibrary.isPaused()) NativeLibrary.pauseEmulation()
            }
            if (intent.action == actionUnmute) {
                if (BooleanSetting.AUDIO_MUTED.boolean) BooleanSetting.AUDIO_MUTED.setBoolean(false)
            } else if (intent.action == actionMute) {
                if (!BooleanSetting.AUDIO_MUTED.boolean) BooleanSetting.AUDIO_MUTED.setBoolean(true)
            }
            buildPictureInPictureParams()
        }
    }

    @SuppressLint("UnspecifiedRegisterReceiverFlag")
    override fun onPictureInPictureModeChanged(
        isInPictureInPictureMode: Boolean,
        newConfig: Configuration
    ) {
        super.onPictureInPictureModeChanged(isInPictureInPictureMode, newConfig)
        if (isInPictureInPictureMode) {
            IntentFilter().apply {
                addAction(actionPause)
                addAction(actionPlay)
                addAction(actionMute)
                addAction(actionUnmute)
            }.also {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
                    registerReceiver(pictureInPictureReceiver, it, RECEIVER_EXPORTED)
                } else {
                    registerReceiver(pictureInPictureReceiver, it)
                }
            }
        } else {
            try {
                unregisterReceiver(pictureInPictureReceiver)
            } catch (ignored: Exception) {
            }
            // Always resume audio, since there is no UI button
            if (BooleanSetting.AUDIO_MUTED.boolean) BooleanSetting.AUDIO_MUTED.setBoolean(false)
        }
    }

    fun onEmulationStarted() {
        emulationViewModel.setEmulationStarted(true)
    }

    fun onEmulationStopped(status: Int) {
        if (status == 0) {
            finish()
        }
    }

    private fun startMotionSensorListener() {
        val sensorManager = this.getSystemService(Context.SENSOR_SERVICE) as SensorManager
        val gyroSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
        val accelSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)
        sensorManager.registerListener(this, gyroSensor, SensorManager.SENSOR_DELAY_GAME)
        sensorManager.registerListener(this, accelSensor, SensorManager.SENSOR_DELAY_GAME)
    }

    private fun stopMotionSensorListener() {
        val sensorManager = this.getSystemService(Context.SENSOR_SERVICE) as SensorManager
        val gyroSensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE)
        val accelSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)

        sensorManager.unregisterListener(this, gyroSensor)
        sensorManager.unregisterListener(this, accelSensor)
    }

    companion object {
        const val EXTRA_SELECTED_GAME = "SelectedGame"

        fun launch(activity: AppCompatActivity, game: Game) {
            val launcher = Intent(activity, EmulationActivity::class.java)
            launcher.putExtra(EXTRA_SELECTED_GAME, game)
            activity.startActivity(launcher)
        }

        fun stopForegroundService(activity: Activity) {
            val startIntent = Intent(activity, ForegroundService::class.java)
            startIntent.action = ForegroundService.ACTION_STOP
            activity.startForegroundService(startIntent)
        }

        private fun areCoordinatesOutside(view: View?, x: Float, y: Float): Boolean {
            if (view == null) {
                return true
            }
            val viewBounds = Rect()
            view.getGlobalVisibleRect(viewBounds)
            return !viewBounds.contains(x.roundToInt(), y.roundToInt())
        }
    }
}
