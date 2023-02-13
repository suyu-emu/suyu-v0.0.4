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

import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.NativeLibrary.ButtonType;
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings;

/**
 * Custom {@link BitmapDrawable} that is capable
 * of storing it's own ID.
 */
public final class InputOverlayDrawableJoystick {
    // The ID value what type of joystick this Drawable represents.
    private int mJoystickId;
    // The ID value what type of button this Drawable represents.
    private int mButtonId;
    // The ID value what motion event is tracking
    private int mTrackId = -1;
    private float mXAxis;
    private float mYAxis;
    private int mControlPositionX, mControlPositionY;
    private int mWidth;
    private int mHeight;
    private Rect mVirtBounds;
    private Rect mOrigBounds;
    private BitmapDrawable mOuterBitmap;
    private BitmapDrawable mDefaultStateInnerBitmap;
    private BitmapDrawable mPressedStateInnerBitmap;
    private BitmapDrawable mBoundsBoxBitmap;
    private boolean mPressedState = false;

    /**
     * Constructor
     *
     * @param res                {@link Resources} instance.
     * @param bitmapOuter        {@link Bitmap} which represents the outer non-movable part of the joystick.
     * @param bitmapInnerDefault {@link Bitmap} which represents the default inner movable part of the joystick.
     * @param bitmapInnerPressed {@link Bitmap} which represents the pressed inner movable part of the joystick.
     * @param rectOuter          {@link Rect} which represents the outer joystick bounds.
     * @param rectInner          {@link Rect} which represents the inner joystick bounds.
     * @param joystick           Identifier for which joystick this is.
     */
    public InputOverlayDrawableJoystick(Resources res, Bitmap bitmapOuter,
                                        Bitmap bitmapInnerDefault, Bitmap bitmapInnerPressed,
                                        Rect rectOuter, Rect rectInner, int joystick, int button) {
        mJoystickId = joystick;
        mButtonId = button;

        mOuterBitmap = new BitmapDrawable(res, bitmapOuter);
        mDefaultStateInnerBitmap = new BitmapDrawable(res, bitmapInnerDefault);
        mPressedStateInnerBitmap = new BitmapDrawable(res, bitmapInnerPressed);
        mBoundsBoxBitmap = new BitmapDrawable(res, bitmapOuter);
        mWidth = bitmapOuter.getWidth();
        mHeight = bitmapOuter.getHeight();

        setBounds(rectOuter);
        mDefaultStateInnerBitmap.setBounds(rectInner);
        mPressedStateInnerBitmap.setBounds(rectInner);
        mVirtBounds = getBounds();
        mOrigBounds = mOuterBitmap.copyBounds();
        mBoundsBoxBitmap.setAlpha(0);
        mBoundsBoxBitmap.setBounds(getVirtBounds());
        SetInnerBounds();
    }

    public void draw(Canvas canvas) {
        mOuterBitmap.draw(canvas);
        getCurrentStateBitmapDrawable().draw(canvas);
        mBoundsBoxBitmap.draw(canvas);
    }

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
            mOuterBitmap.setAlpha(0);
            mBoundsBoxBitmap.setAlpha(255);
            if (EmulationMenuSettings.getJoystickRelCenter()) {
                getVirtBounds().offset(xPosition - getVirtBounds().centerX(),
                        yPosition - getVirtBounds().centerY());
            }
            mBoundsBoxBitmap.setBounds(getVirtBounds());
            mTrackId = pointerId;
        }

        if (isActionUp) {
            if (mTrackId != pointerId) {
                return false;
            }
            mPressedState = false;
            mXAxis = 0.0f;
            mYAxis = 0.0f;
            mOuterBitmap.setAlpha(255);
            mBoundsBoxBitmap.setAlpha(0);
            setVirtBounds(new Rect(mOrigBounds.left, mOrigBounds.top, mOrigBounds.right,
                    mOrigBounds.bottom));
            setBounds(new Rect(mOrigBounds.left, mOrigBounds.top, mOrigBounds.right,
                    mOrigBounds.bottom));
            SetInnerBounds();
            mTrackId = -1;
            return true;
        }

        if (mTrackId == -1)
            return false;

        for (int i = 0; i < event.getPointerCount(); i++) {
            if (mTrackId != event.getPointerId(i)) {
                continue;
            }
            float touchX = event.getX(i);
            float touchY = event.getY(i);
            float maxY = getVirtBounds().bottom;
            float maxX = getVirtBounds().right;
            touchX -= getVirtBounds().centerX();
            maxX -= getVirtBounds().centerX();
            touchY -= getVirtBounds().centerY();
            maxY -= getVirtBounds().centerY();
            final float AxisX = touchX / maxX;
            final float AxisY = touchY / maxY;

            // Clamp the circle pad input to a circle
            final float angle = (float) Math.atan2(AxisY, AxisX);
            float radius = (float) Math.sqrt(AxisX * AxisX + AxisY * AxisY);
            if (radius > 1.0f) {
                radius = 1.0f;
            }
            mXAxis = ((float) Math.cos(angle) * radius);
            mYAxis = ((float) Math.sin(angle) * radius);
            SetInnerBounds();
            return true;
        }

        return false;
    }

    private void SetInnerBounds() {
        int X = getVirtBounds().centerX() + (int) ((mXAxis) * (getVirtBounds().width() / 2));
        int Y = getVirtBounds().centerY() + (int) ((mYAxis) * (getVirtBounds().height() / 2));

        if (X > getVirtBounds().centerX() + (getVirtBounds().width() / 2))
            X = getVirtBounds().centerX() + (getVirtBounds().width() / 2);
        if (X < getVirtBounds().centerX() - (getVirtBounds().width() / 2))
            X = getVirtBounds().centerX() - (getVirtBounds().width() / 2);
        if (Y > getVirtBounds().centerY() + (getVirtBounds().height() / 2))
            Y = getVirtBounds().centerY() + (getVirtBounds().height() / 2);
        if (Y < getVirtBounds().centerY() - (getVirtBounds().height() / 2))
            Y = getVirtBounds().centerY() - (getVirtBounds().height() / 2);

        int width = mPressedStateInnerBitmap.getBounds().width() / 2;
        int height = mPressedStateInnerBitmap.getBounds().height() / 2;
        mDefaultStateInnerBitmap.setBounds(X - width, Y - height, X + width, Y + height);
        mPressedStateInnerBitmap.setBounds(mDefaultStateInnerBitmap.getBounds());
    }

    public void setPosition(int x, int y) {
        mControlPositionX = x;
        mControlPositionY = y;
    }

    private BitmapDrawable getCurrentStateBitmapDrawable() {
        return mPressedState ? mPressedStateInnerBitmap : mDefaultStateInnerBitmap;
    }

    /**
     * Gets this InputOverlayDrawableJoystick's button ID.
     *
     * @return this InputOverlayDrawableJoystick's button ID.
     */
    public int getJoystickId() {
        return mJoystickId;
    }

    public float getXAxis() {
        return mXAxis;
    }

    public float getYAxis() {
        // Nintendo joysticks have y axis inverted
        return -mYAxis;
    }

    public int getButtonId() {
        return mButtonId;
    }

    public int getTrackId() {
        return mTrackId;
    }

    public int getButtonStatus() {
        // TODO: Add button support
        return NativeLibrary.ButtonState.RELEASED;
    }

    public Rect getBounds() {
        return mOuterBitmap.getBounds();
    }

    public void setBounds(Rect bounds) {
        mOuterBitmap.setBounds(bounds);
    }

    private Rect getVirtBounds() {
        return mVirtBounds;
    }

    private void setVirtBounds(Rect bounds) {
        mVirtBounds = bounds;
    }

    public int getWidth() {
        return mWidth;
    }

    public int getHeight() {
        return mHeight;
    }
}
