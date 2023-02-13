/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.yuzu.yuzu_emu.overlay;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.view.MotionEvent;

import org.yuzu.yuzu_emu.NativeLibrary.ButtonState;


/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableButton {
    // The ID value what type of button this Drawable represents.
    private int mButtonId;

    // The ID value what motion event is tracking
    private int mTrackId;

    // The drawable position on the screen
    private int mButtonPositionX, mButtonPositionY;
    private int mWidth;
    private int mHeight;
    private BitmapDrawable mDefaultStateBitmap;
    private BitmapDrawable mPressedStateBitmap;
    private boolean mPressedState = false;

    /**
     * Constructor
     *
     * @param res                {@link Resources} instance.
     * @param defaultStateBitmap {@link Bitmap} to use with the default state Drawable.
     * @param pressedStateBitmap {@link Bitmap} to use with the pressed state Drawable.
     * @param buttonId           Identifier for this type of button.
     */
    public InputOverlayDrawableButton(Resources res, Bitmap defaultStateBitmap,
                                      Bitmap pressedStateBitmap, int buttonId) {
        mDefaultStateBitmap = new BitmapDrawable(res, defaultStateBitmap);
        mPressedStateBitmap = new BitmapDrawable(res, pressedStateBitmap);
        mButtonId = buttonId;
        mTrackId = -1;

        mWidth = mDefaultStateBitmap.getIntrinsicWidth();
        mHeight = mDefaultStateBitmap.getIntrinsicHeight();
    }

    /**
     * Updates button status based on the motion event.
     *
     * @return true if value was changed
     */
    public boolean updateStatus(MotionEvent event) {
        int pointerIndex = event.getActionIndex();
        int xPosition = (int) event.getX(pointerIndex);
        int yPosition = (int) event.getY(pointerIndex);
        int pointerId = event.getPointerId(pointerIndex);
        int motion_event = event.getAction() & MotionEvent.ACTION_MASK;
        boolean isActionDown = motion_event == MotionEvent.ACTION_DOWN || motion_event == MotionEvent.ACTION_POINTER_DOWN;
        boolean isActionUp = motion_event == MotionEvent.ACTION_UP || motion_event == MotionEvent.ACTION_POINTER_UP;

        if (isActionDown) {
            if (!getBounds().contains(xPosition, yPosition)) {
                return false;
            }
            mPressedState = true;
            mTrackId = pointerId;
            return true;
        }

        if (isActionUp) {
            if (mTrackId != pointerId) {
                return false;
            }
            mPressedState = false;
            mTrackId = -1;
            return true;
        }

        return false;
    }

    public void setPosition(int x, int y) {
        mButtonPositionX = x;
        mButtonPositionY = y;
    }

    public void draw(Canvas canvas) {
        getCurrentStateBitmapDrawable().draw(canvas);
    }

    private BitmapDrawable getCurrentStateBitmapDrawable() {
        return mPressedState ? mPressedStateBitmap : mDefaultStateBitmap;
    }

    public void setBounds(int left, int top, int right, int bottom) {
        mDefaultStateBitmap.setBounds(left, top, right, bottom);
        mPressedStateBitmap.setBounds(left, top, right, bottom);
    }

    /**
     * Gets this InputOverlayDrawableButton's button ID.
     *
     * @return this InputOverlayDrawableButton's button ID.
     */
    public int getId() {
        return mButtonId;
    }

    public int getTrackId() {
        return mTrackId;
    }

    public int getStatus() {
        return mPressedState ? ButtonState.PRESSED : ButtonState.RELEASED;
    }

    private Rect getBounds() {
        return mDefaultStateBitmap.getBounds();
    }

    public int getWidth() {
        return mWidth;
    }

    public int getHeight() {
        return mHeight;
    }
}
