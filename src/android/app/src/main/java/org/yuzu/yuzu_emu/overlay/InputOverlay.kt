// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.overlay

import android.app.Activity
import android.content.Context
import android.content.SharedPreferences
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Point
import android.graphics.Rect
import android.graphics.drawable.Drawable
import android.graphics.drawable.VectorDrawable
import android.os.Build
import android.util.AttributeSet
import android.view.HapticFeedbackConstants
import android.view.MotionEvent
import android.view.SurfaceView
import android.view.View
import android.view.View.OnTouchListener
import android.view.WindowInsets
import androidx.core.content.ContextCompat
import androidx.preference.PreferenceManager
import androidx.window.layout.WindowMetricsCalculator
import kotlin.math.max
import kotlin.math.min
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.NativeLibrary.ButtonType
import org.yuzu.yuzu_emu.NativeLibrary.StickType
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings

/**
 * Draws the interactive input overlay on top of the
 * [SurfaceView] that is rendering emulation.
 */
class InputOverlay(context: Context, attrs: AttributeSet?) :
    SurfaceView(context, attrs),
    OnTouchListener {
    private val overlayButtons: MutableSet<InputOverlayDrawableButton> = HashSet()
    private val overlayDpads: MutableSet<InputOverlayDrawableDpad> = HashSet()
    private val overlayJoysticks: MutableSet<InputOverlayDrawableJoystick> = HashSet()

    private var inEditMode = false
    private var buttonBeingConfigured: InputOverlayDrawableButton? = null
    private var dpadBeingConfigured: InputOverlayDrawableDpad? = null
    private var joystickBeingConfigured: InputOverlayDrawableJoystick? = null

    private lateinit var windowInsets: WindowInsets

    var layout = LANDSCAPE

    override fun onLayout(changed: Boolean, left: Int, top: Int, right: Int, bottom: Int) {
        super.onLayout(changed, left, top, right, bottom)

        windowInsets = rootWindowInsets

        val overlayVersion = preferences.getInt(Settings.PREF_OVERLAY_VERSION, 0)
        if (overlayVersion != OVERLAY_VERSION) {
            resetAllLayouts()
        } else {
            val layoutIndex = overlayLayouts.indexOf(layout)
            val currentLayoutVersion =
                preferences.getInt(Settings.overlayLayoutPrefs[layoutIndex], 0)
            if (currentLayoutVersion != overlayLayoutVersions[layoutIndex]) {
                resetCurrentLayout()
            }
        }

        // Load the controls.
        refreshControls()

        // Set the on touch listener.
        setOnTouchListener(this)

        // Force draw
        setWillNotDraw(false)

        // Request focus for the overlay so it has priority on presses.
        requestFocus()
    }

    override fun draw(canvas: Canvas) {
        super.draw(canvas)
        for (button in overlayButtons) {
            button.draw(canvas)
        }
        for (dpad in overlayDpads) {
            dpad.draw(canvas)
        }
        for (joystick in overlayJoysticks) {
            joystick.draw(canvas)
        }
    }

    override fun onTouch(v: View, event: MotionEvent): Boolean {
        if (inEditMode) {
            return onTouchWhileEditing(event)
        }

        var shouldUpdateView = false
        val playerIndex =
            if (NativeLibrary.isHandheldOnly()) {
                NativeLibrary.ConsoleDevice
            } else {
                NativeLibrary.Player1Device
            }

        for (button in overlayButtons) {
            if (!button.updateStatus(event)) {
                continue
            }
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                button.buttonId,
                button.status
            )
            playHaptics(event)
            shouldUpdateView = true
        }

        for (dpad in overlayDpads) {
            if (!dpad.updateStatus(event, EmulationMenuSettings.dpadSlide)) {
                continue
            }
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.upId,
                dpad.upStatus
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.downId,
                dpad.downStatus
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.leftId,
                dpad.leftStatus
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                dpad.rightId,
                dpad.rightStatus
            )
            playHaptics(event)
            shouldUpdateView = true
        }

        for (joystick in overlayJoysticks) {
            if (!joystick.updateStatus(event)) {
                continue
            }
            val axisID = joystick.joystickId
            NativeLibrary.onGamePadJoystickEvent(
                playerIndex,
                axisID,
                joystick.xAxis,
                joystick.realYAxis
            )
            NativeLibrary.onGamePadButtonEvent(
                playerIndex,
                joystick.buttonId,
                joystick.buttonStatus
            )
            playHaptics(event)
            shouldUpdateView = true
        }

        if (shouldUpdateView) {
            invalidate()
        }

        if (!preferences.getBoolean(Settings.PREF_TOUCH_ENABLED, true)) {
            return true
        }

        val pointerIndex = event.actionIndex
        val xPosition = event.getX(pointerIndex).toInt()
        val yPosition = event.getY(pointerIndex).toInt()
        val pointerId = event.getPointerId(pointerIndex)
        val motionEvent = event.action and MotionEvent.ACTION_MASK
        val isActionDown =
            motionEvent == MotionEvent.ACTION_DOWN || motionEvent == MotionEvent.ACTION_POINTER_DOWN
        val isActionMove = motionEvent == MotionEvent.ACTION_MOVE
        val isActionUp =
            motionEvent == MotionEvent.ACTION_UP || motionEvent == MotionEvent.ACTION_POINTER_UP

        if (isActionDown && !isTouchInputConsumed(pointerId)) {
            NativeLibrary.onTouchPressed(pointerId, xPosition.toFloat(), yPosition.toFloat())
        }

        if (isActionMove) {
            for (i in 0 until event.pointerCount) {
                val fingerId = event.getPointerId(i)
                if (isTouchInputConsumed(fingerId)) {
                    continue
                }
                NativeLibrary.onTouchMoved(fingerId, event.getX(i), event.getY(i))
            }
        }

        if (isActionUp && !isTouchInputConsumed(pointerId)) {
            NativeLibrary.onTouchReleased(pointerId)
        }

        return true
    }

    private fun playHaptics(event: MotionEvent) {
        if (EmulationMenuSettings.hapticFeedback) {
            when (event.actionMasked) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY)

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP ->
                    performHapticFeedback(HapticFeedbackConstants.VIRTUAL_KEY_RELEASE)
            }
        }
    }

    private fun isTouchInputConsumed(track_id: Int): Boolean {
        for (button in overlayButtons) {
            if (button.trackId == track_id) {
                return true
            }
        }
        for (dpad in overlayDpads) {
            if (dpad.trackId == track_id) {
                return true
            }
        }
        for (joystick in overlayJoysticks) {
            if (joystick.trackId == track_id) {
                return true
            }
        }
        return false
    }

    private fun onTouchWhileEditing(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val fingerPositionX = event.getX(pointerIndex).toInt()
        val fingerPositionY = event.getY(pointerIndex).toInt()

        for (button in overlayButtons) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    // If no button is being moved now, remember the currently touched button to move.
                    if (buttonBeingConfigured == null &&
                        button.bounds.contains(
                                fingerPositionX,
                                fingerPositionY
                            )
                    ) {
                        buttonBeingConfigured = button
                        buttonBeingConfigured!!.onConfigureTouch(event)
                    }

                MotionEvent.ACTION_MOVE -> if (buttonBeingConfigured != null) {
                    buttonBeingConfigured!!.onConfigureTouch(event)
                    invalidate()
                    return true
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (buttonBeingConfigured === button) {
                    // Persist button position by saving new place.
                    saveControlPosition(
                        buttonBeingConfigured!!.prefId,
                        buttonBeingConfigured!!.bounds.centerX(),
                        buttonBeingConfigured!!.bounds.centerY(),
                        layout
                    )
                    buttonBeingConfigured = null
                }
            }
        }

        for (dpad in overlayDpads) {
            // Determine the button state to apply based on the MotionEvent action flag.
            when (event.action and MotionEvent.ACTION_MASK) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN ->
                    // If no button is being moved now, remember the currently touched button to move.
                    if (buttonBeingConfigured == null &&
                        dpad.bounds.contains(fingerPositionX, fingerPositionY)
                    ) {
                        dpadBeingConfigured = dpad
                        dpadBeingConfigured!!.onConfigureTouch(event)
                    }

                MotionEvent.ACTION_MOVE -> if (dpadBeingConfigured != null) {
                    dpadBeingConfigured!!.onConfigureTouch(event)
                    invalidate()
                    return true
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (dpadBeingConfigured === dpad) {
                    // Persist button position by saving new place.
                    saveControlPosition(
                        Settings.PREF_BUTTON_DPAD,
                        dpadBeingConfigured!!.bounds.centerX(),
                        dpadBeingConfigured!!.bounds.centerY(),
                        layout
                    )
                    dpadBeingConfigured = null
                }
            }
        }

        for (joystick in overlayJoysticks) {
            when (event.action) {
                MotionEvent.ACTION_DOWN,
                MotionEvent.ACTION_POINTER_DOWN -> if (joystickBeingConfigured == null &&
                    joystick.bounds.contains(
                            fingerPositionX,
                            fingerPositionY
                        )
                ) {
                    joystickBeingConfigured = joystick
                    joystickBeingConfigured!!.onConfigureTouch(event)
                }

                MotionEvent.ACTION_MOVE -> if (joystickBeingConfigured != null) {
                    joystickBeingConfigured!!.onConfigureTouch(event)
                    invalidate()
                }

                MotionEvent.ACTION_UP,
                MotionEvent.ACTION_POINTER_UP -> if (joystickBeingConfigured != null) {
                    saveControlPosition(
                        joystickBeingConfigured!!.prefId,
                        joystickBeingConfigured!!.bounds.centerX(),
                        joystickBeingConfigured!!.bounds.centerY(),
                        layout
                    )
                    joystickBeingConfigured = null
                }
            }
        }

        return true
    }

    private fun addOverlayControls(layout: String) {
        val windowSize = getSafeScreenSize(context, Pair(measuredWidth, measuredHeight))
        if (preferences.getBoolean(Settings.PREF_BUTTON_A, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_a,
                    R.drawable.facebutton_a_depressed,
                    ButtonType.BUTTON_A,
                    Settings.PREF_BUTTON_A,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_B, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_b,
                    R.drawable.facebutton_b_depressed,
                    ButtonType.BUTTON_B,
                    Settings.PREF_BUTTON_B,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_X, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_x,
                    R.drawable.facebutton_x_depressed,
                    ButtonType.BUTTON_X,
                    Settings.PREF_BUTTON_X,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_Y, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_y,
                    R.drawable.facebutton_y_depressed,
                    ButtonType.BUTTON_Y,
                    Settings.PREF_BUTTON_Y,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_L, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.l_shoulder,
                    R.drawable.l_shoulder_depressed,
                    ButtonType.TRIGGER_L,
                    Settings.PREF_BUTTON_L,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_R, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.r_shoulder,
                    R.drawable.r_shoulder_depressed,
                    ButtonType.TRIGGER_R,
                    Settings.PREF_BUTTON_R,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_ZL, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.zl_trigger,
                    R.drawable.zl_trigger_depressed,
                    ButtonType.TRIGGER_ZL,
                    Settings.PREF_BUTTON_ZL,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_ZR, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.zr_trigger,
                    R.drawable.zr_trigger_depressed,
                    ButtonType.TRIGGER_ZR,
                    Settings.PREF_BUTTON_ZR,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_PLUS, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_plus,
                    R.drawable.facebutton_plus_depressed,
                    ButtonType.BUTTON_PLUS,
                    Settings.PREF_BUTTON_PLUS,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_MINUS, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_minus,
                    R.drawable.facebutton_minus_depressed,
                    ButtonType.BUTTON_MINUS,
                    Settings.PREF_BUTTON_MINUS,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_DPAD, true)) {
            overlayDpads.add(
                initializeOverlayDpad(
                    context,
                    windowSize,
                    R.drawable.dpad_standard,
                    R.drawable.dpad_standard_cardinal_depressed,
                    R.drawable.dpad_standard_diagonal_depressed,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_STICK_L, true)) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    windowSize,
                    R.drawable.joystick_range,
                    R.drawable.joystick,
                    R.drawable.joystick_depressed,
                    StickType.STICK_L,
                    ButtonType.STICK_L,
                    Settings.PREF_STICK_L,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_STICK_R, true)) {
            overlayJoysticks.add(
                initializeOverlayJoystick(
                    context,
                    windowSize,
                    R.drawable.joystick_range,
                    R.drawable.joystick,
                    R.drawable.joystick_depressed,
                    StickType.STICK_R,
                    ButtonType.STICK_R,
                    Settings.PREF_STICK_R,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_HOME, false)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_home,
                    R.drawable.facebutton_home_depressed,
                    ButtonType.BUTTON_HOME,
                    Settings.PREF_BUTTON_HOME,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_SCREENSHOT, false)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.facebutton_screenshot,
                    R.drawable.facebutton_screenshot_depressed,
                    ButtonType.BUTTON_CAPTURE,
                    Settings.PREF_BUTTON_SCREENSHOT,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_STICK_L, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.button_l3,
                    R.drawable.button_l3_depressed,
                    ButtonType.STICK_L,
                    Settings.PREF_BUTTON_STICK_L,
                    layout
                )
            )
        }
        if (preferences.getBoolean(Settings.PREF_BUTTON_STICK_R, true)) {
            overlayButtons.add(
                initializeOverlayButton(
                    context,
                    windowSize,
                    R.drawable.button_r3,
                    R.drawable.button_r3_depressed,
                    ButtonType.STICK_R,
                    Settings.PREF_BUTTON_STICK_R,
                    layout
                )
            )
        }
    }

    fun refreshControls() {
        // Remove all the overlay buttons from the HashSet.
        overlayButtons.clear()
        overlayDpads.clear()
        overlayJoysticks.clear()

        // Add all the enabled overlay items back to the HashSet.
        if (EmulationMenuSettings.showOverlay) {
            addOverlayControls(layout)
        }
        invalidate()
    }

    private fun saveControlPosition(prefId: String, x: Int, y: Int, layout: String) {
        val windowSize = getSafeScreenSize(context, Pair(measuredWidth, measuredHeight))
        val min = windowSize.first
        val max = windowSize.second
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext).edit()
            .putFloat("$prefId-X$layout", (x - min.x).toFloat() / max.x)
            .putFloat("$prefId-Y$layout", (y - min.y).toFloat() / max.y)
            .apply()
    }

    fun setIsInEditMode(editMode: Boolean) {
        inEditMode = editMode
    }

    private fun resetCurrentLayout() {
        defaultOverlayByLayout(layout)
        val layoutIndex = overlayLayouts.indexOf(layout)
        preferences.edit()
            .putInt(Settings.overlayLayoutPrefs[layoutIndex], overlayLayoutVersions[layoutIndex])
            .apply()
    }

    private fun resetAllLayouts() {
        val editor = preferences.edit()
        overlayLayouts.forEachIndexed { i, layout ->
            defaultOverlayByLayout(layout)
            editor.putInt(Settings.overlayLayoutPrefs[i], overlayLayoutVersions[i])
        }
        editor.putInt(Settings.PREF_OVERLAY_VERSION, OVERLAY_VERSION)
        editor.apply()
    }

    fun resetLayoutVisibilityAndPlacement() {
        defaultOverlayByLayout(layout)
        val editor = preferences.edit()
        Settings.overlayPreferences.forEachIndexed { _, pref ->
            editor.remove(pref)
        }
        editor.apply()
        refreshControls()
    }

    private val landscapeResources = arrayOf(
        R.integer.SWITCH_BUTTON_A_X,
        R.integer.SWITCH_BUTTON_A_Y,
        R.integer.SWITCH_BUTTON_B_X,
        R.integer.SWITCH_BUTTON_B_Y,
        R.integer.SWITCH_BUTTON_X_X,
        R.integer.SWITCH_BUTTON_X_Y,
        R.integer.SWITCH_BUTTON_Y_X,
        R.integer.SWITCH_BUTTON_Y_Y,
        R.integer.SWITCH_TRIGGER_ZL_X,
        R.integer.SWITCH_TRIGGER_ZL_Y,
        R.integer.SWITCH_TRIGGER_ZR_X,
        R.integer.SWITCH_TRIGGER_ZR_Y,
        R.integer.SWITCH_BUTTON_DPAD_X,
        R.integer.SWITCH_BUTTON_DPAD_Y,
        R.integer.SWITCH_TRIGGER_L_X,
        R.integer.SWITCH_TRIGGER_L_Y,
        R.integer.SWITCH_TRIGGER_R_X,
        R.integer.SWITCH_TRIGGER_R_Y,
        R.integer.SWITCH_BUTTON_PLUS_X,
        R.integer.SWITCH_BUTTON_PLUS_Y,
        R.integer.SWITCH_BUTTON_MINUS_X,
        R.integer.SWITCH_BUTTON_MINUS_Y,
        R.integer.SWITCH_BUTTON_HOME_X,
        R.integer.SWITCH_BUTTON_HOME_Y,
        R.integer.SWITCH_BUTTON_CAPTURE_X,
        R.integer.SWITCH_BUTTON_CAPTURE_Y,
        R.integer.SWITCH_STICK_R_X,
        R.integer.SWITCH_STICK_R_Y,
        R.integer.SWITCH_STICK_L_X,
        R.integer.SWITCH_STICK_L_Y,
        R.integer.SWITCH_BUTTON_STICK_L_X,
        R.integer.SWITCH_BUTTON_STICK_L_Y,
        R.integer.SWITCH_BUTTON_STICK_R_X,
        R.integer.SWITCH_BUTTON_STICK_R_Y
    )

    private val portraitResources = arrayOf(
        R.integer.SWITCH_BUTTON_A_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_A_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_B_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_B_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_X_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_X_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_Y_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_Y_Y_PORTRAIT,
        R.integer.SWITCH_TRIGGER_ZL_X_PORTRAIT,
        R.integer.SWITCH_TRIGGER_ZL_Y_PORTRAIT,
        R.integer.SWITCH_TRIGGER_ZR_X_PORTRAIT,
        R.integer.SWITCH_TRIGGER_ZR_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_DPAD_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_DPAD_Y_PORTRAIT,
        R.integer.SWITCH_TRIGGER_L_X_PORTRAIT,
        R.integer.SWITCH_TRIGGER_L_Y_PORTRAIT,
        R.integer.SWITCH_TRIGGER_R_X_PORTRAIT,
        R.integer.SWITCH_TRIGGER_R_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_PLUS_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_PLUS_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_MINUS_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_MINUS_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_HOME_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_HOME_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_CAPTURE_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_CAPTURE_Y_PORTRAIT,
        R.integer.SWITCH_STICK_R_X_PORTRAIT,
        R.integer.SWITCH_STICK_R_Y_PORTRAIT,
        R.integer.SWITCH_STICK_L_X_PORTRAIT,
        R.integer.SWITCH_STICK_L_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_STICK_L_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_STICK_L_Y_PORTRAIT,
        R.integer.SWITCH_BUTTON_STICK_R_X_PORTRAIT,
        R.integer.SWITCH_BUTTON_STICK_R_Y_PORTRAIT
    )

    private val foldableResources = arrayOf(
        R.integer.SWITCH_BUTTON_A_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_A_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_B_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_B_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_X_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_X_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_Y_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_Y_Y_FOLDABLE,
        R.integer.SWITCH_TRIGGER_ZL_X_FOLDABLE,
        R.integer.SWITCH_TRIGGER_ZL_Y_FOLDABLE,
        R.integer.SWITCH_TRIGGER_ZR_X_FOLDABLE,
        R.integer.SWITCH_TRIGGER_ZR_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_DPAD_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_DPAD_Y_FOLDABLE,
        R.integer.SWITCH_TRIGGER_L_X_FOLDABLE,
        R.integer.SWITCH_TRIGGER_L_Y_FOLDABLE,
        R.integer.SWITCH_TRIGGER_R_X_FOLDABLE,
        R.integer.SWITCH_TRIGGER_R_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_PLUS_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_PLUS_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_MINUS_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_MINUS_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_HOME_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_HOME_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_CAPTURE_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_CAPTURE_Y_FOLDABLE,
        R.integer.SWITCH_STICK_R_X_FOLDABLE,
        R.integer.SWITCH_STICK_R_Y_FOLDABLE,
        R.integer.SWITCH_STICK_L_X_FOLDABLE,
        R.integer.SWITCH_STICK_L_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_STICK_L_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_STICK_L_Y_FOLDABLE,
        R.integer.SWITCH_BUTTON_STICK_R_X_FOLDABLE,
        R.integer.SWITCH_BUTTON_STICK_R_Y_FOLDABLE
    )

    private fun getResourceValue(layout: String, position: Int): Float {
        return when (layout) {
            PORTRAIT -> resources.getInteger(portraitResources[position]).toFloat() / 1000
            FOLDABLE -> resources.getInteger(foldableResources[position]).toFloat() / 1000
            else -> resources.getInteger(landscapeResources[position]).toFloat() / 1000
        }
    }

    private fun defaultOverlayByLayout(layout: String) {
        // Each value represents the position of the button in relation to the screen size without insets.
        preferences.edit()
            .putFloat(
                "${Settings.PREF_BUTTON_A}-X$layout",
                getResourceValue(layout, 0)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_A}-Y$layout",
                getResourceValue(layout, 1)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_B}-X$layout",
                getResourceValue(layout, 2)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_B}-Y$layout",
                getResourceValue(layout, 3)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_X}-X$layout",
                getResourceValue(layout, 4)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_X}-Y$layout",
                getResourceValue(layout, 5)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_Y}-X$layout",
                getResourceValue(layout, 6)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_Y}-Y$layout",
                getResourceValue(layout, 7)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_ZL}-X$layout",
                getResourceValue(layout, 8)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_ZL}-Y$layout",
                getResourceValue(layout, 9)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_ZR}-X$layout",
                getResourceValue(layout, 10)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_ZR}-Y$layout",
                getResourceValue(layout, 11)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_DPAD}-X$layout",
                getResourceValue(layout, 12)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_DPAD}-Y$layout",
                getResourceValue(layout, 13)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_L}-X$layout",
                getResourceValue(layout, 14)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_L}-Y$layout",
                getResourceValue(layout, 15)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_R}-X$layout",
                getResourceValue(layout, 16)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_R}-Y$layout",
                getResourceValue(layout, 17)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_PLUS}-X$layout",
                getResourceValue(layout, 18)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_PLUS}-Y$layout",
                getResourceValue(layout, 19)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_MINUS}-X$layout",
                getResourceValue(layout, 20)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_MINUS}-Y$layout",
                getResourceValue(layout, 21)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_HOME}-X$layout",
                getResourceValue(layout, 22)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_HOME}-Y$layout",
                getResourceValue(layout, 23)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_SCREENSHOT}-X$layout",
                getResourceValue(layout, 24)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_SCREENSHOT}-Y$layout",
                getResourceValue(layout, 25)
            )
            .putFloat(
                "${Settings.PREF_STICK_R}-X$layout",
                getResourceValue(layout, 26)
            )
            .putFloat(
                "${Settings.PREF_STICK_R}-Y$layout",
                getResourceValue(layout, 27)
            )
            .putFloat(
                "${Settings.PREF_STICK_L}-X$layout",
                getResourceValue(layout, 28)
            )
            .putFloat(
                "${Settings.PREF_STICK_L}-Y$layout",
                getResourceValue(layout, 29)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_STICK_L}-X$layout",
                getResourceValue(layout, 30)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_STICK_L}-Y$layout",
                getResourceValue(layout, 31)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_STICK_R}-X$layout",
                getResourceValue(layout, 32)
            )
            .putFloat(
                "${Settings.PREF_BUTTON_STICK_R}-Y$layout",
                getResourceValue(layout, 33)
            )
            .apply()
    }

    override fun isInEditMode(): Boolean {
        return inEditMode
    }

    companion object {
        // Increase this number every time there is a breaking change to every overlay layout
        const val OVERLAY_VERSION = 1

        // Increase the corresponding layout version number whenever that layout has a breaking change
        private const val LANDSCAPE_OVERLAY_VERSION = 1
        private const val PORTRAIT_OVERLAY_VERSION = 1
        private const val FOLDABLE_OVERLAY_VERSION = 1
        val overlayLayoutVersions = listOf(
            LANDSCAPE_OVERLAY_VERSION,
            PORTRAIT_OVERLAY_VERSION,
            FOLDABLE_OVERLAY_VERSION
        )

        private val preferences: SharedPreferences =
            PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

        const val LANDSCAPE = "_Landscape"
        const val PORTRAIT = "_Portrait"
        const val FOLDABLE = "_Foldable"
        val overlayLayouts = listOf(
            LANDSCAPE,
            PORTRAIT,
            FOLDABLE
        )

        /**
         * Resizes a [Bitmap] by a given scale factor
         *
         * @param context       Context for getting the vector drawable
         * @param drawableId    The ID of the drawable to scale.
         * @param scale         The scale factor for the bitmap.
         * @return The scaled [Bitmap]
         */
        private fun getBitmap(context: Context, drawableId: Int, scale: Float): Bitmap {
            val vectorDrawable = ContextCompat.getDrawable(context, drawableId) as VectorDrawable

            val bitmap = Bitmap.createBitmap(
                (vectorDrawable.intrinsicWidth * scale).toInt(),
                (vectorDrawable.intrinsicHeight * scale).toInt(),
                Bitmap.Config.ARGB_8888
            )

            val dm = context.resources.displayMetrics
            val minScreenDimension = min(dm.widthPixels, dm.heightPixels)

            val maxBitmapDimension = max(bitmap.width, bitmap.height)
            val bitmapScale = scale * minScreenDimension / maxBitmapDimension

            val scaledBitmap = Bitmap.createScaledBitmap(
                bitmap,
                (bitmap.width * bitmapScale).toInt(),
                (bitmap.height * bitmapScale).toInt(),
                true
            )

            val canvas = Canvas(scaledBitmap)
            vectorDrawable.setBounds(0, 0, canvas.width, canvas.height)
            vectorDrawable.draw(canvas)
            return scaledBitmap
        }

        /**
         * Gets the safe screen size for drawing the overlay
         *
         * @param context   Context for getting the window metrics
         * @return A pair of points, the first being the top left corner of the safe area,
         *                  the second being the bottom right corner of the safe area
         */
        private fun getSafeScreenSize(
            context: Context,
            screenSize: Pair<Int, Int>
        ): Pair<Point, Point> {
            // Get screen size
            val windowMetrics = WindowMetricsCalculator.getOrCreate()
                .computeCurrentWindowMetrics(context as Activity)
            var maxX = screenSize.first.toFloat()
            var maxY = screenSize.second.toFloat()
            var minX = 0
            var minY = 0

            // If we have API access, calculate the safe area to draw the overlay
            var cutoutLeft = 0
            var cutoutBottom = 0
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                val insets = context.windowManager.currentWindowMetrics.windowInsets.displayCutout
                if (insets != null) {
                    if (insets.boundingRectTop.bottom != 0 &&
                        insets.boundingRectTop.bottom > maxY / 2
                    ) {
                        maxY = insets.boundingRectTop.bottom.toFloat()
                    }
                    if (insets.boundingRectRight.left != 0 &&
                        insets.boundingRectRight.left > maxX / 2
                    ) {
                        maxX = insets.boundingRectRight.left.toFloat()
                    }

                    minX = insets.boundingRectLeft.right - insets.boundingRectLeft.left
                    minY = insets.boundingRectBottom.top - insets.boundingRectBottom.bottom

                    cutoutLeft = insets.boundingRectRight.right - insets.boundingRectRight.left
                    cutoutBottom = insets.boundingRectTop.top - insets.boundingRectTop.bottom
                }
            }

            // This makes sure that if we have an inset on one side of the screen, we mirror it on
            // the other side. Since removing space from one of the max values messes with the scale,
            // we also have to account for it using our min values.
            if (maxX.toInt() != windowMetrics.bounds.width()) minX += cutoutLeft
            if (maxY.toInt() != windowMetrics.bounds.height()) minY += cutoutBottom
            if (minX > 0 && maxX.toInt() == windowMetrics.bounds.width()) {
                maxX -= (minX * 2)
            } else if (minX > 0) {
                maxX -= minX
            }
            if (minY > 0 && maxY.toInt() == windowMetrics.bounds.height()) {
                maxY -= (minY * 2)
            } else if (minY > 0) {
                maxY -= minY
            }

            return Pair(Point(minX, minY), Point(maxX.toInt(), maxY.toInt()))
        }

        /**
         * Initializes an InputOverlayDrawableButton, given by resId, with all of the
         * parameters set for it to be properly shown on the InputOverlay.
         *
         *
         * This works due to the way the X and Y coordinates are stored within
         * the [SharedPreferences].
         *
         *
         * In the input overlay configuration menu,
         * once a touch event begins and then ends (ie. Organizing the buttons to one's own liking for the overlay).
         * the X and Y coordinates of the button at the END of its touch event
         * (when you remove your finger/stylus from the touchscreen) are then stored
         * within a SharedPreferences instance so that those values can be retrieved here.
         *
         *
         * This has a few benefits over the conventional way of storing the values
         * (ie. within the yuzu ini file).
         *
         *  * No native calls
         *  * Keeps Android-only values inside the Android environment
         *
         *
         *
         * Technically no modifications should need to be performed on the returned
         * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
         * for Android to call the onDraw method.
         *
         * @param context      The current [Context].
         * @param windowSize   The size of the window to draw the overlay on.
         * @param defaultResId The resource ID of the [Drawable] to get the [Bitmap] of (Default State).
         * @param pressedResId The resource ID of the [Drawable] to get the [Bitmap] of (Pressed State).
         * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
         * @param prefId       Identifier for determining where a button appears on screen.
         * @param layout       The current screen layout as determined by [LANDSCAPE], [PORTRAIT], or [FOLDABLE].
         * @return An [InputOverlayDrawableButton] with the correct drawing bounds set.
         */
        private fun initializeOverlayButton(
            context: Context,
            windowSize: Pair<Point, Point>,
            defaultResId: Int,
            pressedResId: Int,
            buttonId: Int,
            prefId: String,
            layout: String
        ): InputOverlayDrawableButton {
            // Resources handle for fetching the initial Drawable resource.
            val res = context.resources

            // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
            val sPrefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

            // Decide scale based on button preference ID and user preference
            var scale: Float = when (prefId) {
                Settings.PREF_BUTTON_HOME,
                Settings.PREF_BUTTON_SCREENSHOT,
                Settings.PREF_BUTTON_PLUS,
                Settings.PREF_BUTTON_MINUS -> 0.07f

                Settings.PREF_BUTTON_L,
                Settings.PREF_BUTTON_R,
                Settings.PREF_BUTTON_ZL,
                Settings.PREF_BUTTON_ZR -> 0.26f

                Settings.PREF_BUTTON_STICK_L,
                Settings.PREF_BUTTON_STICK_R -> 0.155f

                else -> 0.11f
            }
            scale *= (sPrefs.getInt(Settings.PREF_CONTROL_SCALE, 50) + 50).toFloat()
            scale /= 100f

            // Initialize the InputOverlayDrawableButton.
            val defaultStateBitmap = getBitmap(context, defaultResId, scale)
            val pressedStateBitmap = getBitmap(context, pressedResId, scale)
            val overlayDrawable = InputOverlayDrawableButton(
                res,
                defaultStateBitmap,
                pressedStateBitmap,
                buttonId,
                prefId
            )

            // Get the minimum and maximum coordinates of the screen where the button can be placed.
            val min = windowSize.first
            val max = windowSize.second

            // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
            // These were set in the input overlay configuration menu.
            val xKey = "$prefId-X$layout"
            val yKey = "$prefId-Y$layout"
            val drawableXPercent = sPrefs.getFloat(xKey, 0f)
            val drawableYPercent = sPrefs.getFloat(yKey, 0f)
            val drawableX = (drawableXPercent * max.x + min.x).toInt()
            val drawableY = (drawableYPercent * max.y + min.y).toInt()
            val width = overlayDrawable.width
            val height = overlayDrawable.height

            // Now set the bounds for the InputOverlayDrawableButton.
            // This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
            overlayDrawable.setBounds(
                drawableX - (width / 2),
                drawableY - (height / 2),
                drawableX + (width / 2),
                drawableY + (height / 2)
            )

            // Need to set the image's position
            overlayDrawable.setPosition(
                drawableX - (width / 2),
                drawableY - (height / 2)
            )
            val savedOpacity = preferences.getInt(Settings.PREF_CONTROL_OPACITY, 100)
            overlayDrawable.setOpacity(savedOpacity * 255 / 100)
            return overlayDrawable
        }

        /**
         * Initializes an [InputOverlayDrawableDpad]
         *
         * @param context                   The current [Context].
         * @param windowSize                The size of the window to draw the overlay on.
         * @param defaultResId              The [Bitmap] resource ID of the default state.
         * @param pressedOneDirectionResId  The [Bitmap] resource ID of the pressed state in one direction.
         * @param pressedTwoDirectionsResId The [Bitmap] resource ID of the pressed state in two directions.
         * @param layout                    The current screen layout as determined by [LANDSCAPE], [PORTRAIT], or [FOLDABLE].
         * @return The initialized [InputOverlayDrawableDpad]
         */
        private fun initializeOverlayDpad(
            context: Context,
            windowSize: Pair<Point, Point>,
            defaultResId: Int,
            pressedOneDirectionResId: Int,
            pressedTwoDirectionsResId: Int,
            layout: String
        ): InputOverlayDrawableDpad {
            // Resources handle for fetching the initial Drawable resource.
            val res = context.resources

            // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
            val sPrefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

            // Decide scale based on button ID and user preference
            var scale = 0.25f
            scale *= (sPrefs.getInt(Settings.PREF_CONTROL_SCALE, 50) + 50).toFloat()
            scale /= 100f

            // Initialize the InputOverlayDrawableDpad.
            val defaultStateBitmap =
                getBitmap(context, defaultResId, scale)
            val pressedOneDirectionStateBitmap = getBitmap(context, pressedOneDirectionResId, scale)
            val pressedTwoDirectionsStateBitmap =
                getBitmap(context, pressedTwoDirectionsResId, scale)

            val overlayDrawable = InputOverlayDrawableDpad(
                res,
                defaultStateBitmap,
                pressedOneDirectionStateBitmap,
                pressedTwoDirectionsStateBitmap,
                ButtonType.DPAD_UP,
                ButtonType.DPAD_DOWN,
                ButtonType.DPAD_LEFT,
                ButtonType.DPAD_RIGHT
            )

            // Get the minimum and maximum coordinates of the screen where the button can be placed.
            val min = windowSize.first
            val max = windowSize.second

            // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
            // These were set in the input overlay configuration menu.
            val drawableXPercent = sPrefs.getFloat("${Settings.PREF_BUTTON_DPAD}-X$layout", 0f)
            val drawableYPercent = sPrefs.getFloat("${Settings.PREF_BUTTON_DPAD}-Y$layout", 0f)
            val drawableX = (drawableXPercent * max.x + min.x).toInt()
            val drawableY = (drawableYPercent * max.y + min.y).toInt()
            val width = overlayDrawable.width
            val height = overlayDrawable.height

            // Now set the bounds for the InputOverlayDrawableDpad.
            // This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
            overlayDrawable.setBounds(
                drawableX - (width / 2),
                drawableY - (height / 2),
                drawableX + (width / 2),
                drawableY + (height / 2)
            )

            // Need to set the image's position
            overlayDrawable.setPosition(drawableX - (width / 2), drawableY - (height / 2))
            val savedOpacity = preferences.getInt(Settings.PREF_CONTROL_OPACITY, 100)
            overlayDrawable.setOpacity(savedOpacity * 255 / 100)
            return overlayDrawable
        }

        /**
         * Initializes an [InputOverlayDrawableJoystick]
         *
         * @param context         The current [Context]
         * @param windowSize      The size of the window to draw the overlay on.
         * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
         * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
         * @param pressedResInner Resource ID for the pressed inner image of the joystick.
         * @param joystick        Identifier for which joystick this is.
         * @param button          Identifier for which joystick button this is.
         * @param prefId          Identifier for determining where a button appears on screen.
         * @param layout          The current screen layout as determined by [LANDSCAPE], [PORTRAIT], or [FOLDABLE].
         * @return The initialized [InputOverlayDrawableJoystick].
         */
        private fun initializeOverlayJoystick(
            context: Context,
            windowSize: Pair<Point, Point>,
            resOuter: Int,
            defaultResInner: Int,
            pressedResInner: Int,
            joystick: Int,
            button: Int,
            prefId: String,
            layout: String
        ): InputOverlayDrawableJoystick {
            // Resources handle for fetching the initial Drawable resource.
            val res = context.resources

            // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
            val sPrefs = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

            // Decide scale based on user preference
            var scale = 0.3f
            scale *= (sPrefs.getInt(Settings.PREF_CONTROL_SCALE, 50) + 50).toFloat()
            scale /= 100f

            // Initialize the InputOverlayDrawableJoystick.
            val bitmapOuter = getBitmap(context, resOuter, scale)
            val bitmapInnerDefault = getBitmap(context, defaultResInner, 1.0f)
            val bitmapInnerPressed = getBitmap(context, pressedResInner, 1.0f)

            // Get the minimum and maximum coordinates of the screen where the button can be placed.
            val min = windowSize.first
            val max = windowSize.second

            // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
            // These were set in the input overlay configuration menu.
            val drawableXPercent = sPrefs.getFloat("$prefId-X$layout", 0f)
            val drawableYPercent = sPrefs.getFloat("$prefId-Y$layout", 0f)
            val drawableX = (drawableXPercent * max.x + min.x).toInt()
            val drawableY = (drawableYPercent * max.y + min.y).toInt()
            val outerScale = 1.66f

            // Now set the bounds for the InputOverlayDrawableJoystick.
            // This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
            val outerSize = bitmapOuter.width
            val outerRect = Rect(
                drawableX - (outerSize / 2),
                drawableY - (outerSize / 2),
                drawableX + (outerSize / 2),
                drawableY + (outerSize / 2)
            )
            val innerRect =
                Rect(0, 0, (outerSize / outerScale).toInt(), (outerSize / outerScale).toInt())

            // Send the drawableId to the joystick so it can be referenced when saving control position.
            val overlayDrawable = InputOverlayDrawableJoystick(
                res,
                bitmapOuter,
                bitmapInnerDefault,
                bitmapInnerPressed,
                outerRect,
                innerRect,
                joystick,
                button,
                prefId
            )

            // Need to set the image's position
            overlayDrawable.setPosition(drawableX, drawableY)
            val savedOpacity = preferences.getInt(Settings.PREF_CONTROL_OPACITY, 100)
            overlayDrawable.setOpacity(savedOpacity * 255 / 100)
            return overlayDrawable
        }
    }
}
