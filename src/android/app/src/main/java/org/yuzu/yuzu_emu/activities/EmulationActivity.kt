// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.activities

import android.app.Activity
import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.graphics.Rect
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.hardware.display.DisplayManager
import android.os.Bundle
import android.view.Display
import android.view.InputDevice
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.Surface
import android.view.View
import android.view.inputmethod.InputMethodManager
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.getSystemService
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.window.layout.WindowInfoTracker
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.SettingsViewModel
import org.yuzu.yuzu_emu.fragments.EmulationFragment
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.ControllerMappingHelper
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings
import org.yuzu.yuzu_emu.utils.ForegroundService
import org.yuzu.yuzu_emu.utils.InputHandler
import org.yuzu.yuzu_emu.utils.NfcReader
import org.yuzu.yuzu_emu.utils.SerializableHelper.parcelable
import org.yuzu.yuzu_emu.utils.ThemeHelper
import kotlin.math.roundToInt

class EmulationActivity : AppCompatActivity(), SensorEventListener {
    private var controllerMappingHelper: ControllerMappingHelper? = null

    var isActivityRecreated = false
    private var emulationFragment: EmulationFragment? = null
    private lateinit var nfcReader: NfcReader
    private lateinit var inputHandler: InputHandler

    private val gyro = FloatArray(3)
    private val accel = FloatArray(3)
    private var motionTimestamp: Long = 0
    private var flipMotionOrientation: Boolean = false

    private lateinit var game: Game

    private val settingsViewModel: SettingsViewModel by viewModels()

    override fun onDestroy() {
        stopForegroundService(this)
        super.onDestroy()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        settingsViewModel.settings.loadSettings()

        super.onCreate(savedInstanceState)
        if (savedInstanceState == null) {
            // Get params we were passed
            game = intent.parcelable(EXTRA_SELECTED_GAME)!!
            isActivityRecreated = false
        } else {
            isActivityRecreated = true
            restoreState(savedInstanceState)
        }
        controllerMappingHelper = ControllerMappingHelper()

        // Set these options now so that the SurfaceView the game renders into is the right size.
        enableFullscreenImmersive()

        setContentView(R.layout.activity_emulation)
        window.decorView.setBackgroundColor(getColor(android.R.color.black))

        // Find or create the EmulationFragment
        emulationFragment =
            supportFragmentManager.findFragmentById(R.id.frame_emulation_fragment) as EmulationFragment?
        if (emulationFragment == null) {
            emulationFragment = EmulationFragment.newInstance(game)
            supportFragmentManager.beginTransaction()
                .add(R.id.frame_emulation_fragment, emulationFragment!!)
                .commit()
        }
        title = game.title

        nfcReader = NfcReader(this)
        nfcReader.initialize()

        inputHandler = InputHandler()
        inputHandler.initialize()

        lifecycleScope.launch(Dispatchers.Main) {
            lifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                WindowInfoTracker.getOrCreate(this@EmulationActivity)
                    .windowLayoutInfo(this@EmulationActivity)
                    .collect { emulationFragment?.updateCurrentLayout(this@EmulationActivity, it) }
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

        NativeLibrary.notifyOrientationChange(
            EmulationMenuSettings.landscapeScreenLayout,
            getAdjustedRotation()
        )
    }

    override fun onPause() {
        super.onPause()
        nfcReader.stopScanning()
        stopMotionSensorListener()
    }

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        setIntent(intent)
        nfcReader.onNewIntent(intent)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putParcelable(EXTRA_SELECTED_GAME, game)
        super.onSaveInstanceState(outState)
    }

    override fun dispatchKeyEvent(event: KeyEvent): Boolean {
        if (event.source and InputDevice.SOURCE_JOYSTICK != InputDevice.SOURCE_JOYSTICK &&
            event.source and InputDevice.SOURCE_GAMEPAD != InputDevice.SOURCE_GAMEPAD
        ) {
            return super.dispatchKeyEvent(event)
        }

        return inputHandler.dispatchKeyEvent(event)
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

        return inputHandler.dispatchGenericMotionEvent(event)
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

    private fun getAdjustedRotation():Int {
        val rotation = getSystemService<DisplayManager>()!!.getDisplay(Display.DEFAULT_DISPLAY).rotation
        val config: Configuration = resources.configuration

        if ((config.screenLayout and Configuration.SCREENLAYOUT_LONG_YES) != 0 ||
            (config.screenLayout and Configuration.SCREENLAYOUT_LONG_NO) == 0) {
            return rotation
        }
        when (rotation) {
            Surface.ROTATION_0 -> return Surface.ROTATION_90
            Surface.ROTATION_90 -> return Surface.ROTATION_0
            Surface.ROTATION_180 -> return Surface.ROTATION_270
            Surface.ROTATION_270 -> return Surface.ROTATION_180
        }
        return rotation
    }

    private fun restoreState(savedInstanceState: Bundle) {
        game = savedInstanceState.parcelable(EXTRA_SELECTED_GAME)!!
    }

    private fun enableFullscreenImmersive() {
        WindowCompat.setDecorFitsSystemWindows(window, false)

        WindowInsetsControllerCompat(window, window.decorView).let { controller ->
            controller.hide(WindowInsetsCompat.Type.systemBars())
            controller.systemBarsBehavior =
                WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
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
