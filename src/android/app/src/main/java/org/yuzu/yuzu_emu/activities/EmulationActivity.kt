// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.activities

import android.app.Activity
import android.content.DialogInterface
import android.content.Intent
import android.graphics.Rect
import android.os.Bundle
import android.view.*
import android.view.KeyEvent
import android.view.MotionEvent
import android.view.inputmethod.InputMethodManager
import androidx.appcompat.app.AppCompatActivity
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider.OnChangeListener
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogSliderBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.fragments.EmulationFragment
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.utils.ControllerMappingHelper
import org.yuzu.yuzu_emu.utils.InputHandler
import org.yuzu.yuzu_emu.utils.NfcReader
import org.yuzu.yuzu_emu.utils.SerializableHelper.parcelable
import org.yuzu.yuzu_emu.utils.ThemeHelper
import kotlin.math.roundToInt

open class EmulationActivity : AppCompatActivity() {
    private var controllerMappingHelper: ControllerMappingHelper? = null

    // TODO(bunnei): Disable notifications until we support app suspension.
    //private Intent foregroundService;

    var isActivityRecreated = false
    private var menuVisible = false
    private var emulationFragment: EmulationFragment? = null
    private lateinit var nfcReader: NfcReader
    private lateinit var inputHandler: InputHandler

    private lateinit var game: Game

    override fun onDestroy() {
        // TODO(bunnei): Disable notifications until we support app suspension.
        //stopService(foregroundService);
        super.onDestroy()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

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

        // Start a foreground service to prevent the app from getting killed in the background
        // TODO(bunnei): Disable notifications until we support app suspension.
        //foregroundService = new Intent(EmulationActivity.this, ForegroundService.class);
        //startForegroundService(foregroundService);
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
        // Handling the case where the back button is pressed.
        if (event.keyCode == KeyEvent.KEYCODE_BACK) {
            onBackPressedDispatcher.onBackPressed()
            return true
        }

        return inputHandler.dispatchKeyEvent(event)
    }

    override fun dispatchGenericMotionEvent(event: MotionEvent): Boolean {
        if (event.source and InputDevice.SOURCE_CLASS_JOYSTICK === 0) {
            return super.dispatchGenericMotionEvent(event)
        }

        // Don't attempt to do anything if we are disconnecting a device.
        if (event.actionMasked == MotionEvent.ACTION_CANCEL) {
            return true
        }

        return inputHandler.dispatchGenericMotionEvent(event)
    }

    private fun restoreState(savedInstanceState: Bundle) {
        game = savedInstanceState.parcelable(EXTRA_SELECTED_GAME)!!
    }

    private fun enableFullscreenImmersive() {
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES

        window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN)

        // It would be nice to use IMMERSIVE_STICKY, but that doesn't show the toolbar.
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_IMMERSIVE
    }

    private fun editControlsPlacement() {
        if (emulationFragment!!.isConfiguringControls) {
            emulationFragment!!.stopConfiguringControls()
        } else {
            emulationFragment!!.startConfiguringControls()
        }
    }

    private fun adjustScale() {
        val sliderBinding = DialogSliderBinding.inflate(layoutInflater)
        sliderBinding.slider.valueTo = 150F
        sliderBinding.slider.value =
            PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getInt(Settings.PREF_CONTROL_SCALE, 50).toFloat()
        sliderBinding.slider.addOnChangeListener(OnChangeListener { _, value, _ ->
            sliderBinding.textValue.text = value.toString()
            setControlScale(value.toInt())
        })
        sliderBinding.textValue.text = sliderBinding.slider.value.toString()
        sliderBinding.textUnits.text = "%"
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_control_scale)
            .setView(sliderBinding.root)
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                setControlScale(sliderBinding.slider.value.toInt())
            }
            .setNeutralButton(R.string.slider_default) { _: DialogInterface?, _: Int ->
                setControlScale(50)
            }
            .show()
    }

    private fun setControlScale(scale: Int) {
        PreferenceManager.getDefaultSharedPreferences(applicationContext).edit()
            .putInt(Settings.PREF_CONTROL_SCALE, scale)
            .apply()
        emulationFragment!!.refreshInputOverlay()
    }

    private fun resetOverlay() {
        MaterialAlertDialogBuilder(this)
            .setTitle(getString(R.string.emulation_touch_overlay_reset))
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int -> emulationFragment!!.resetInputOverlay() }
            .setNegativeButton(android.R.string.cancel, null)
            .create()
            .show()
    }

    companion object {
        const val EXTRA_SELECTED_GAME = "SelectedGame"
        private const val EMULATION_RUNNING_NOTIFICATION = 0x1000

        @JvmStatic
        fun launch(activity: AppCompatActivity, game: Game) {
            val launcher = Intent(activity, EmulationActivity::class.java)
            launcher.putExtra(EXTRA_SELECTED_GAME, game)
            activity.startActivity(launcher)
        }

        @JvmStatic
        fun tryDismissRunningNotification(activity: Activity?) {
            // TODO(bunnei): Disable notifications until we support app suspension.
            //NotificationManagerCompat.from(activity).cancel(EMULATION_RUNNING_NOTIFICATION);
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
