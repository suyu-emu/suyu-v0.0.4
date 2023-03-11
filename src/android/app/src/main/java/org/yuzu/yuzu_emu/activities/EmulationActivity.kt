// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.activities

import android.app.Activity
import android.content.DialogInterface
import android.content.Intent
import android.graphics.Rect
import android.os.Bundle
import android.view.LayoutInflater
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.annotation.IntDef
import androidx.appcompat.app.AppCompatActivity
import androidx.fragment.app.Fragment
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.FragmentManager
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.slider.Slider
import com.google.android.material.slider.Slider.OnChangeListener
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.fragments.EmulationFragment
import org.yuzu.yuzu_emu.fragments.MenuFragment
import org.yuzu.yuzu_emu.utils.ControllerMappingHelper
import kotlin.math.roundToInt

open class EmulationActivity : AppCompatActivity() {
    private var controllerMappingHelper: ControllerMappingHelper? = null

    // TODO(bunnei): Disable notifications until we support app suspension.
    //private Intent foregroundService;

    var isActivityRecreated = false
    private var selectedTitle: String? = null
    private var path: String? = null
    private var menuVisible = false
    private var emulationFragment: EmulationFragment? = null

    override fun onDestroy() {
        // TODO(bunnei): Disable notifications until we support app suspension.
        //stopService(foregroundService);
        super.onDestroy()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        if (savedInstanceState == null) {
            // Get params we were passed
            val gameToEmulate = intent
            path = gameToEmulate.getStringExtra(EXTRA_SELECTED_GAME)
            selectedTitle = gameToEmulate.getStringExtra(EXTRA_SELECTED_TITLE)
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
        var emulationFragment =
            supportFragmentManager.findFragmentById(R.id.frame_emulation_fragment) as EmulationFragment?
        if (emulationFragment == null) {
            emulationFragment = EmulationFragment.newInstance(path)
            supportFragmentManager.beginTransaction()
                .add(R.id.frame_emulation_fragment, emulationFragment)
                .commit()
        }
        title = selectedTitle

        // Start a foreground service to prevent the app from getting killed in the background
        // TODO(bunnei): Disable notifications until we support app suspension.
        //foregroundService = new Intent(EmulationActivity.this, ForegroundService.class);
        //startForegroundService(foregroundService);

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                toggleMenu()
            }
        })
    }

    override fun onSaveInstanceState(outState: Bundle) {
        outState.putString(EXTRA_SELECTED_GAME, path)
        outState.putString(EXTRA_SELECTED_TITLE, selectedTitle)
        super.onSaveInstanceState(outState)
    }

    private fun restoreState(savedInstanceState: Bundle) {
        path = savedInstanceState.getString(EXTRA_SELECTED_GAME)
        selectedTitle = savedInstanceState.getString(EXTRA_SELECTED_TITLE)

        // If an alert prompt was in progress when state was restored, retry displaying it
        NativeLibrary.retryDisplayAlertPrompt()
    }

    private fun enableFullscreenImmersive() {
        window.attributes.layoutInDisplayCutoutMode =
            WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES

        // It would be nice to use IMMERSIVE_STICKY, but that doesn't show the toolbar.
        window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or
                View.SYSTEM_UI_FLAG_HIDE_NAVIGATION or
                View.SYSTEM_UI_FLAG_FULLSCREEN or
                View.SYSTEM_UI_FLAG_IMMERSIVE
    }

    fun handleMenuAction(action: Int) {
        when (action) {
            MENU_ACTION_EXIT -> {
                emulationFragment!!.stopEmulation()
                finish()
            }
        }
    }

    private fun editControlsPlacement() {
        if (emulationFragment!!.isConfiguringControls) {
            emulationFragment!!.stopConfiguringControls()
        } else {
            emulationFragment!!.startConfiguringControls()
        }
    }

    private fun adjustScale() {
        val inflater = LayoutInflater.from(this)
        val view = inflater.inflate(R.layout.dialog_slider, null)
        val slider = view.findViewById<Slider>(R.id.slider)
        val textValue = view.findViewById<TextView>(R.id.text_value)
        val units = view.findViewById<TextView>(R.id.text_units)

        slider.valueTo = 150F
        slider.value = PreferenceManager.getDefaultSharedPreferences(applicationContext)
            .getInt(Settings.PREF_CONTROL_SCALE, 50).toFloat()
        slider.addOnChangeListener(OnChangeListener { _, value, _ ->
            textValue.text = value.toString()
            setControlScale(value.toInt())
        })
        textValue.text = slider.value.toString()
        units.text = "%"
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.emulation_control_scale)
            .setView(view)
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface?, _: Int ->
                setControlScale(slider.value.toInt())
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

    override fun dispatchTouchEvent(event: MotionEvent): Boolean {
        if (event.actionMasked == MotionEvent.ACTION_DOWN) {
            var anyMenuClosed = false
            var submenu = supportFragmentManager.findFragmentById(R.id.frame_submenu)
            if (submenu != null && areCoordinatesOutside(submenu.view, event.x, event.y)) {
                closeSubmenu()
                submenu = null
                anyMenuClosed = true
            }
            if (submenu == null) {
                val menu = supportFragmentManager.findFragmentById(R.id.frame_menu)
                if (menu != null && areCoordinatesOutside(menu.view, event.x, event.y)) {
                    closeMenu()
                    anyMenuClosed = true
                }
            }
            if (anyMenuClosed) {
                return true
            }
        }
        return super.dispatchTouchEvent(event)
    }

    @Retention(AnnotationRetention.SOURCE)
    @IntDef(
        MENU_ACTION_EDIT_CONTROLS_PLACEMENT,
        MENU_ACTION_TOGGLE_CONTROLS,
        MENU_ACTION_ADJUST_SCALE,
        MENU_ACTION_EXIT,
        MENU_ACTION_SHOW_FPS,
        MENU_ACTION_RESET_OVERLAY,
        MENU_ACTION_SHOW_OVERLAY,
        MENU_ACTION_OPEN_SETTINGS
    )
    annotation class MenuAction

    private fun closeSubmenu(): Boolean {
        return supportFragmentManager.popBackStackImmediate(
            BACKSTACK_NAME_SUBMENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE
        )
    }

    private fun closeMenu(): Boolean {
        menuVisible = false
        return supportFragmentManager.popBackStackImmediate(
            BACKSTACK_NAME_MENU,
            FragmentManager.POP_BACK_STACK_INCLUSIVE
        )
    }

    private fun toggleMenu() {
        if (!closeMenu()) {
            val fragment: Fragment = MenuFragment.newInstance()
            supportFragmentManager.beginTransaction()
                .setCustomAnimations(
                    R.animator.menu_slide_in_from_start,
                    R.animator.menu_slide_out_to_start,
                    R.animator.menu_slide_in_from_start,
                    R.animator.menu_slide_out_to_start
                )
                .add(R.id.frame_menu, fragment)
                .addToBackStack(BACKSTACK_NAME_MENU)
                .commit()
            menuVisible = true
        }
    }

    companion object {
        private const val BACKSTACK_NAME_MENU = "menu"
        private const val BACKSTACK_NAME_SUBMENU = "submenu"
        const val EXTRA_SELECTED_GAME = "SelectedGame"
        const val EXTRA_SELECTED_TITLE = "SelectedTitle"
        const val MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0
        const val MENU_ACTION_TOGGLE_CONTROLS = 1
        const val MENU_ACTION_ADJUST_SCALE = 2
        const val MENU_ACTION_EXIT = 3
        const val MENU_ACTION_SHOW_FPS = 4
        const val MENU_ACTION_RESET_OVERLAY = 6
        const val MENU_ACTION_SHOW_OVERLAY = 7
        const val MENU_ACTION_OPEN_SETTINGS = 8
        private const val EMULATION_RUNNING_NOTIFICATION = 0x1000

        @JvmStatic
        fun launch(activity: FragmentActivity, path: String?, title: String?) {
            val launcher = Intent(activity, EmulationActivity::class.java)
            launcher.putExtra(EXTRA_SELECTED_GAME, path)
            launcher.putExtra(EXTRA_SELECTED_TITLE, title)
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
