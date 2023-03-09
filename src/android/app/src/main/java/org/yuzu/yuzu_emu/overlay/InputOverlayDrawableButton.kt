/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */
package org.yuzu.yuzu_emu.overlay

import android.content.res.Resources
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Rect
import android.graphics.drawable.BitmapDrawable
import android.view.MotionEvent
import org.yuzu.yuzu_emu.NativeLibrary.ButtonState

/**
 * Custom [BitmapDrawable] that is capable
 * of storing it's own ID.
 *
 * @param res                [Resources] instance.
 * @param defaultStateBitmap [Bitmap] to use with the default state Drawable.
 * @param pressedStateBitmap [Bitmap] to use with the pressed state Drawable.
 * @param buttonId           Identifier for this type of button.
 */
class InputOverlayDrawableButton(
    res: Resources,
    defaultStateBitmap: Bitmap,
    pressedStateBitmap: Bitmap,
    buttonId: Int
) {
    /**
     * Gets this InputOverlayDrawableButton's button ID.
     *
     * @return this InputOverlayDrawableButton's button ID.
     */
    // The ID value what type of button this Drawable represents.
    val id: Int

    // The ID value what motion event is tracking
    var trackId: Int

    // The drawable position on the screen
    private var buttonPositionX = 0
    private var buttonPositionY = 0
    val width: Int
    val height: Int
    private val defaultStateBitmap: BitmapDrawable
    private val pressedStateBitmap: BitmapDrawable
    private var pressedState = false

    init {
        this.defaultStateBitmap = BitmapDrawable(res, defaultStateBitmap)
        this.pressedStateBitmap = BitmapDrawable(res, pressedStateBitmap)
        id = buttonId
        trackId = -1
        width = this.defaultStateBitmap.intrinsicWidth
        height = this.defaultStateBitmap.intrinsicHeight
    }

    /**
     * Updates button status based on the motion event.
     *
     * @return true if value was changed
     */
    fun updateStatus(event: MotionEvent): Boolean {
        val pointerIndex = event.actionIndex
        val xPosition = event.getX(pointerIndex).toInt()
        val yPosition = event.getY(pointerIndex).toInt()
        val pointerId = event.getPointerId(pointerIndex)
        val motionEvent = event.action and MotionEvent.ACTION_MASK
        val isActionDown =
            motionEvent == MotionEvent.ACTION_DOWN || motionEvent == MotionEvent.ACTION_POINTER_DOWN
        val isActionUp =
            motionEvent == MotionEvent.ACTION_UP || motionEvent == MotionEvent.ACTION_POINTER_UP

        if (isActionDown) {
            if (!bounds.contains(xPosition, yPosition)) {
                return false
            }
            pressedState = true
            trackId = pointerId
            return true
        }

        if (isActionUp) {
            if (trackId != pointerId) {
                return false
            }
            pressedState = false
            trackId = -1
            return true
        }

        return false
    }

    fun setPosition(x: Int, y: Int) {
        buttonPositionX = x
        buttonPositionY = y
    }

    fun draw(canvas: Canvas?) {
        currentStateBitmapDrawable.draw(canvas!!)
    }

    private val currentStateBitmapDrawable: BitmapDrawable
        get() = if (pressedState) pressedStateBitmap else defaultStateBitmap

    fun setBounds(left: Int, top: Int, right: Int, bottom: Int) {
        defaultStateBitmap.setBounds(left, top, right, bottom)
        pressedStateBitmap.setBounds(left, top, right, bottom)
    }

    val status: Int
        get() = if (pressedState) ButtonState.PRESSED else ButtonState.RELEASED
    private val bounds: Rect
        get() = defaultStateBitmap.bounds
}
