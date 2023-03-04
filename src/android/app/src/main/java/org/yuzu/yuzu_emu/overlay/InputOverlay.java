/**
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.yuzu.yuzu_emu.overlay;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Rect;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.VectorDrawable;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.preference.PreferenceManager;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.Display;
import android.view.MotionEvent;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnTouchListener;

import androidx.core.content.ContextCompat;

import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.NativeLibrary.ButtonType;
import org.yuzu.yuzu_emu.NativeLibrary.StickType;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings;

import java.util.HashSet;
import java.util.Set;

/**
 * Draws the interactive input overlay on top of the
 * {@link SurfaceView} that is rendering emulation.
 */
public final class InputOverlay extends SurfaceView implements OnTouchListener, SensorEventListener {
    private final Set<InputOverlayDrawableButton> overlayButtons = new HashSet<>();
    private final Set<InputOverlayDrawableDpad> overlayDpads = new HashSet<>();
    private final Set<InputOverlayDrawableJoystick> overlayJoysticks = new HashSet<>();

    private boolean mIsInEditMode = false;

    private SharedPreferences mPreferences;

    private float[] gyro = new float[3];
    private float[] accel = new float[3];

    private long motionTimestamp;

    /**
     * Constructor
     *
     * @param context The current {@link Context}.
     * @param attrs   {@link AttributeSet} for parsing XML attributes.
     */
    public InputOverlay(Context context, AttributeSet attrs) {
        super(context, attrs);

        mPreferences = PreferenceManager.getDefaultSharedPreferences(getContext());
        if (!mPreferences.getBoolean("OverlayInit", false)) {
            defaultOverlay();
        }

        // Load the controls.
        refreshControls();

        // Set the on motion sensor listener.
        setMotionSensorListener(context);

        // Set the on touch listener.
        setOnTouchListener(this);

        // Force draw
        setWillNotDraw(false);

        // Request focus for the overlay so it has priority on presses.
        requestFocus();
    }

    private void setMotionSensorListener(Context context) {
        SensorManager sensorManager = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);
        Sensor gyro_sensor = sensorManager.getDefaultSensor(Sensor.TYPE_GYROSCOPE);
        Sensor accel_sensor = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);

        if (gyro_sensor != null) {
            sensorManager.registerListener(this, gyro_sensor, SensorManager.SENSOR_DELAY_GAME);
        }
        if (accel_sensor != null) {
            sensorManager.registerListener(this, accel_sensor, SensorManager.SENSOR_DELAY_GAME);
        }
    }


    /**
     * Resizes a {@link Bitmap} by a given scale factor
     *
     * @param vectorDrawable The {@link Bitmap} to scale.
     * @param scale          The scale factor for the bitmap.
     * @return The scaled {@link Bitmap}
     */
    private static Bitmap getBitmap(VectorDrawable vectorDrawable, float scale) {
        Bitmap bitmap = Bitmap.createBitmap((int) (vectorDrawable.getIntrinsicWidth() * scale),
                (int) (vectorDrawable.getIntrinsicHeight() * scale), Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        vectorDrawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        vectorDrawable.draw(canvas);
        return bitmap;
    }

    private static Bitmap getBitmap(Context context, int drawableId, float scale) {
        Drawable drawable = ContextCompat.getDrawable(context, drawableId);
        if (drawable instanceof BitmapDrawable) {
            return BitmapFactory.decodeResource(context.getResources(), drawableId);
        } else if (drawable instanceof VectorDrawable) {
            return getBitmap((VectorDrawable) drawable, scale);
        } else {
            throw new IllegalArgumentException("unsupported drawable type");
        }
    }

    /**
     * Initializes an InputOverlayDrawableButton, given by resId, with all of the
     * parameters set for it to be properly shown on the InputOverlay.
     * <p>
     * This works due to the way the X and Y coordinates are stored within
     * the {@link SharedPreferences}.
     * <p>
     * In the input overlay configuration menu,
     * once a touch event begins and then ends (ie. Organizing the buttons to one's own liking for the overlay).
     * the X and Y coordinates of the button at the END of its touch event
     * (when you remove your finger/stylus from the touchscreen) are then stored
     * within a SharedPreferences instance so that those values can be retrieved here.
     * <p>
     * This has a few benefits over the conventional way of storing the values
     * (ie. within the yuzu ini file).
     * <ul>
     * <li>No native calls</li>
     * <li>Keeps Android-only values inside the Android environment</li>
     * </ul>
     * <p>
     * Technically no modifications should need to be performed on the returned
     * InputOverlayDrawableButton. Simply add it to the HashSet of overlay items and wait
     * for Android to call the onDraw method.
     *
     * @param context      The current {@link Context}.
     * @param defaultResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Default State).
     * @param pressedResId The resource ID of the {@link Drawable} to get the {@link Bitmap} of (Pressed State).
     * @param buttonId     Identifier for determining what type of button the initialized InputOverlayDrawableButton represents.
     * @return An {@link InputOverlayDrawableButton} with the correct drawing bounds set.
     */
    private static InputOverlayDrawableButton initializeOverlayButton(Context context,
                                                                      int defaultResId, int pressedResId, int buttonId, String orientation) {
        // Resources handle for fetching the initial Drawable resource.
        final Resources res = context.getResources();

        // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableButton.
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // Decide scale based on button ID and user preference
        float scale;

        switch (buttonId) {
            case ButtonType.BUTTON_HOME:
            case ButtonType.BUTTON_CAPTURE:
            case ButtonType.BUTTON_PLUS:
            case ButtonType.BUTTON_MINUS:
                scale = 0.35f;
                break;
            case ButtonType.TRIGGER_L:
            case ButtonType.TRIGGER_R:
            case ButtonType.TRIGGER_ZL:
            case ButtonType.TRIGGER_ZR:
                scale = 0.38f;
                break;
            default:
                scale = 0.40f;
                break;
        }

        scale *= (sPrefs.getInt("controlScale", 50) + 50);
        scale /= 100;

        // Initialize the InputOverlayDrawableButton.
        final Bitmap defaultStateBitmap = getBitmap(context, defaultResId, scale);
        final Bitmap pressedStateBitmap = getBitmap(context, pressedResId, scale);
        final InputOverlayDrawableButton overlayDrawable =
                new InputOverlayDrawableButton(res, defaultStateBitmap, pressedStateBitmap, buttonId);

        // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
        // These were set in the input overlay configuration menu.
        String xKey;
        String yKey;

        xKey = buttonId + orientation + "-X";
        yKey = buttonId + orientation + "-Y";

        int drawableX = (int) sPrefs.getFloat(xKey, 0f);
        int drawableY = (int) sPrefs.getFloat(yKey, 0f);

        int width = overlayDrawable.getWidth();
        int height = overlayDrawable.getHeight();

        // Now set the bounds for the InputOverlayDrawableButton.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableButton will be.
        overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height);

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY);

        return overlayDrawable;
    }

    /**
     * Initializes an {@link InputOverlayDrawableDpad}
     *
     * @param context                   The current {@link Context}.
     * @param defaultResId              The {@link Bitmap} resource ID of the default sate.
     * @param pressedOneDirectionResId  The {@link Bitmap} resource ID of the pressed sate in one direction.
     * @param pressedTwoDirectionsResId The {@link Bitmap} resource ID of the pressed sate in two directions.
     * @param buttonUp                  Identifier for the up button.
     * @param buttonDown                Identifier for the down button.
     * @param buttonLeft                Identifier for the left button.
     * @param buttonRight               Identifier for the right button.
     * @return the initialized {@link InputOverlayDrawableDpad}
     */
    private static InputOverlayDrawableDpad initializeOverlayDpad(Context context,
                                                                  int defaultResId,
                                                                  int pressedOneDirectionResId,
                                                                  int pressedTwoDirectionsResId,
                                                                  int buttonUp,
                                                                  int buttonDown,
                                                                  int buttonLeft,
                                                                  int buttonRight,
                                                                  String orientation) {
        // Resources handle for fetching the initial Drawable resource.
        final Resources res = context.getResources();

        // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableDpad.
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // Decide scale based on button ID and user preference
        float scale = 0.40f;

        scale *= (sPrefs.getInt("controlScale", 50) + 50);
        scale /= 100;

        // Initialize the InputOverlayDrawableDpad.
        final Bitmap defaultStateBitmap = getBitmap(context, defaultResId, scale);
        final Bitmap pressedOneDirectionStateBitmap = getBitmap(context, pressedOneDirectionResId,
                scale);
        final Bitmap pressedTwoDirectionsStateBitmap = getBitmap(context, pressedTwoDirectionsResId,
                scale);
        final InputOverlayDrawableDpad overlayDrawable =
                new InputOverlayDrawableDpad(res, defaultStateBitmap,
                        pressedOneDirectionStateBitmap, pressedTwoDirectionsStateBitmap,
                        buttonUp, buttonDown, buttonLeft, buttonRight);

        // The X and Y coordinates of the InputOverlayDrawableDpad on the InputOverlay.
        // These were set in the input overlay configuration menu.
        int drawableX = (int) sPrefs.getFloat(buttonUp + orientation + "-X", 0f);
        int drawableY = (int) sPrefs.getFloat(buttonUp + orientation + "-Y", 0f);

        int width = overlayDrawable.getWidth();
        int height = overlayDrawable.getHeight();

        // Now set the bounds for the InputOverlayDrawableDpad.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableDpad will be.
        overlayDrawable.setBounds(drawableX, drawableY, drawableX + width, drawableY + height);

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY);

        return overlayDrawable;
    }

    /**
     * Initializes an {@link InputOverlayDrawableJoystick}
     *
     * @param context         The current {@link Context}
     * @param resOuter        Resource ID for the outer image of the joystick (the static image that shows the circular bounds).
     * @param defaultResInner Resource ID for the default inner image of the joystick (the one you actually move around).
     * @param pressedResInner Resource ID for the pressed inner image of the joystick.
     * @param joystick        Identifier for which joystick this is.
     * @param button          Identifier for which joystick button this is.
     * @return the initialized {@link InputOverlayDrawableJoystick}.
     */
    private static InputOverlayDrawableJoystick initializeOverlayJoystick(Context context,
                                                                          int resOuter, int defaultResInner, int pressedResInner, int joystick, int button, String orientation) {
        // Resources handle for fetching the initial Drawable resource.
        final Resources res = context.getResources();

        // SharedPreference to retrieve the X and Y coordinates for the InputOverlayDrawableJoystick.
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(context);

        // Decide scale based on user preference
        float scale = 0.35f;
        scale *= (sPrefs.getInt("controlScale", 50) + 50);
        scale /= 100;

        // Initialize the InputOverlayDrawableJoystick.
        final Bitmap bitmapOuter = getBitmap(context, resOuter, scale);
        final Bitmap bitmapInnerDefault = getBitmap(context, defaultResInner, 1.0f);
        final Bitmap bitmapInnerPressed = getBitmap(context, pressedResInner, 1.0f);

        // The X and Y coordinates of the InputOverlayDrawableButton on the InputOverlay.
        // These were set in the input overlay configuration menu.
        int drawableX = (int) sPrefs.getFloat(button + orientation + "-X", 0f);
        int drawableY = (int) sPrefs.getFloat(button + orientation + "-Y", 0f);

        float outerScale = 1.3f;

        // Now set the bounds for the InputOverlayDrawableJoystick.
        // This will dictate where on the screen (and the what the size) the InputOverlayDrawableJoystick will be.
        int outerSize = bitmapOuter.getWidth();
        Rect outerRect = new Rect(drawableX, drawableY, drawableX + outerSize, drawableY + outerSize);
        Rect innerRect = new Rect(0, 0, (int) (outerSize / outerScale), (int) (outerSize / outerScale));

        // Send the drawableId to the joystick so it can be referenced when saving control position.
        final InputOverlayDrawableJoystick overlayDrawable
                = new InputOverlayDrawableJoystick(res, bitmapOuter,
                bitmapInnerDefault, bitmapInnerPressed,
                outerRect, innerRect, joystick, button);

        // Need to set the image's position
        overlayDrawable.setPosition(drawableX, drawableY);

        return overlayDrawable;
    }

    @Override
    public void draw(Canvas canvas) {
        super.draw(canvas);

        for (InputOverlayDrawableButton button : overlayButtons) {
            button.draw(canvas);
        }

        for (InputOverlayDrawableDpad dpad : overlayDpads) {
            dpad.draw(canvas);
        }

        for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
            joystick.draw(canvas);
        }
    }

    @Override
    public boolean onTouch(View v, MotionEvent event) {
        if (isInEditMode()) {
            return onTouchWhileEditing(event);
        }

        for (InputOverlayDrawableButton button : overlayButtons) {
            if (!button.updateStatus(event)) {
                continue;
            }
            NativeLibrary.onGamePadButtonEvent(NativeLibrary.Player1Device, button.getId(), button.getStatus());
        }

        for (InputOverlayDrawableDpad dpad : overlayDpads) {
            if (!dpad.updateStatus(event, EmulationMenuSettings.getDpadSlideEnable())) {
                continue;
            }
            NativeLibrary.onGamePadButtonEvent(NativeLibrary.Player1Device, dpad.getUpId(), dpad.getUpStatus());
            NativeLibrary.onGamePadButtonEvent(NativeLibrary.Player1Device, dpad.getDownId(), dpad.getDownStatus());
            NativeLibrary.onGamePadButtonEvent(NativeLibrary.Player1Device, dpad.getLeftId(), dpad.getLeftStatus());
            NativeLibrary.onGamePadButtonEvent(NativeLibrary.Player1Device, dpad.getRightId(), dpad.getRightStatus());
        }

        for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
            if (!joystick.updateStatus(event)) {
                continue;
            }
            int axisID = joystick.getJoystickId();
            NativeLibrary.onGamePadJoystickEvent(NativeLibrary.Player1Device, axisID, joystick.getXAxis(), joystick.getYAxis());
            NativeLibrary.onGamePadButtonEvent(NativeLibrary.Player1Device, joystick.getButtonId(), joystick.getButtonStatus());
        }

        if (!mPreferences.getBoolean("isTouchEnabled", true)) {
            return true;
        }

        int pointerIndex = event.getActionIndex();
        int xPosition = (int) event.getX(pointerIndex);
        int yPosition = (int) event.getY(pointerIndex);
        int pointerId = event.getPointerId(pointerIndex);
        int motion_event = event.getAction() & MotionEvent.ACTION_MASK;
        boolean isActionDown = motion_event == MotionEvent.ACTION_DOWN || motion_event == MotionEvent.ACTION_POINTER_DOWN;
        boolean isActionMove = motion_event == MotionEvent.ACTION_MOVE;
        boolean isActionUp = motion_event == MotionEvent.ACTION_UP || motion_event == MotionEvent.ACTION_POINTER_UP;

        if (isActionDown && !isTouchInputConsumed(pointerId)) {
            NativeLibrary.onTouchPressed(pointerId, xPosition, yPosition);
        }

        if (isActionMove) {
            for (int i = 0; i < event.getPointerCount(); i++) {
                int fingerId = event.getPointerId(i);
                if (isTouchInputConsumed(fingerId)) {
                    continue;
                }
                NativeLibrary.onTouchMoved(fingerId, event.getX(i), event.getY(i));
            }
        }

        if (isActionUp && !isTouchInputConsumed(pointerId)) {
            NativeLibrary.onTouchReleased(pointerId);
        }

        invalidate();

        return true;
    }

    private boolean isTouchInputConsumed(int track_id) {
        for (InputOverlayDrawableButton button : overlayButtons) {
            if (button.getTrackId() == track_id) {
                return true;
            }
        }
        for (InputOverlayDrawableDpad dpad : overlayDpads) {
            if (dpad.getTrackId() == track_id) {
                return true;
            }
        }
        for (InputOverlayDrawableJoystick joystick : overlayJoysticks) {
            if (joystick.getTrackId() == track_id) {
                return true;
            }
        }
        return false;
    }

    public boolean onTouchWhileEditing(MotionEvent event) {
        // TODO: Reimplement this
        return true;
    }

    @Override
    public void onSensorChanged(SensorEvent event) {
        if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
            accel[0] = -event.values[1] / SensorManager.GRAVITY_EARTH;
            accel[1] = event.values[0] / SensorManager.GRAVITY_EARTH;
            accel[2] = -event.values[2] / SensorManager.GRAVITY_EARTH;
        }

        if (event.sensor.getType() == Sensor.TYPE_GYROSCOPE) {
            // Investigate why sensor value is off by 12x
            gyro[0] = event.values[1] / 12.0f;
            gyro[1] = -event.values[0] / 12.0f;
            gyro[2] = event.values[2] / 12.0f;
        }

        // Only update state on accelerometer data
        if (event.sensor.getType() != Sensor.TYPE_ACCELEROMETER) {
            return;
        }

        long delta_timestamp = (event.timestamp - motionTimestamp) / 1000;
        motionTimestamp = event.timestamp;
        NativeLibrary.onGamePadMotionEvent(NativeLibrary.Player1Device, delta_timestamp, gyro[0], gyro[1], gyro[2], accel[0], accel[1], accel[2]);
        NativeLibrary.onGamePadMotionEvent(NativeLibrary.ConsoleDevice, delta_timestamp, gyro[0], gyro[1], gyro[2], accel[0], accel[1], accel[2]);
    }

    @Override
    public void onAccuracyChanged(Sensor sensor, int i) {
    }

    private void addOverlayControls(String orientation) {
        if (mPreferences.getBoolean("buttonToggle0", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_a,
                    R.drawable.facebutton_a_depressed, ButtonType.BUTTON_A, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle1", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_b,
                    R.drawable.facebutton_b_depressed, ButtonType.BUTTON_B, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle2", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_x,
                    R.drawable.facebutton_x_depressed, ButtonType.BUTTON_X, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle3", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_y,
                    R.drawable.facebutton_y_depressed, ButtonType.BUTTON_Y, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle4", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.l_shoulder,
                    R.drawable.l_shoulder_depressed, ButtonType.TRIGGER_L, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle5", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.r_shoulder,
                    R.drawable.r_shoulder_depressed, ButtonType.TRIGGER_R, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle6", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.zl_trigger,
                    R.drawable.zl_trigger_depressed, ButtonType.TRIGGER_ZL, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle7", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.zr_trigger,
                    R.drawable.zr_trigger_depressed, ButtonType.TRIGGER_ZR, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle8", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_plus,
                    R.drawable.facebutton_plus_depressed, ButtonType.BUTTON_PLUS, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle9", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_minus,
                    R.drawable.facebutton_minus_depressed, ButtonType.BUTTON_MINUS, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle10", true)) {
            overlayDpads.add(initializeOverlayDpad(getContext(), R.drawable.dpad_standard,
                    R.drawable.dpad_standard_cardinal_depressed,
                    R.drawable.dpad_standard_diagonal_depressed,
                    ButtonType.DPAD_UP, ButtonType.DPAD_DOWN,
                    ButtonType.DPAD_LEFT, ButtonType.DPAD_RIGHT, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle11", true)) {
            overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.joystick_range,
                    R.drawable.joystick, R.drawable.joystick_depressed,
                    StickType.STICK_L, ButtonType.STICK_L, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle12", true)) {
            overlayJoysticks.add(initializeOverlayJoystick(getContext(), R.drawable.joystick_range,
                    R.drawable.joystick, R.drawable.joystick_depressed, StickType.STICK_R, ButtonType.STICK_R, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle13", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_home,
                    R.drawable.facebutton_home_depressed, ButtonType.BUTTON_HOME, orientation));
        }
        if (mPreferences.getBoolean("buttonToggle14", true)) {
            overlayButtons.add(initializeOverlayButton(getContext(), R.drawable.facebutton_screenshot,
                    R.drawable.facebutton_screenshot_depressed, ButtonType.BUTTON_CAPTURE, orientation));
        }
    }

    public void refreshControls() {
        // Remove all the overlay buttons from the HashSet.
        overlayButtons.clear();
        overlayDpads.clear();
        overlayJoysticks.clear();

        String orientation =
                getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT ?
                        "-Portrait" : "";

        // Add all the enabled overlay items back to the HashSet.
        if (EmulationMenuSettings.getShowOverlay()) {
            addOverlayControls(orientation);
        }

        invalidate();
    }

    private void saveControlPosition(int sharedPrefsId, int x, int y, String orientation) {
        final SharedPreferences sPrefs = PreferenceManager.getDefaultSharedPreferences(getContext());
        SharedPreferences.Editor sPrefsEditor = sPrefs.edit();
        sPrefsEditor.putFloat(sharedPrefsId + orientation + "-X", x);
        sPrefsEditor.putFloat(sharedPrefsId + orientation + "-Y", y);
        sPrefsEditor.apply();
    }

    public void setIsInEditMode(boolean isInEditMode) {
        mIsInEditMode = isInEditMode;
    }

    private void defaultOverlay() {
        if (!mPreferences.getBoolean("OverlayInit", false)) {
            defaultOverlayLandscape();
        }

        SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
        sPrefsEditor.putBoolean("OverlayInit", true);
        sPrefsEditor.apply();
    }

    public void resetButtonPlacement() {
        defaultOverlayLandscape();
        refreshControls();
    }

    private void defaultOverlayLandscape() {
        SharedPreferences.Editor sPrefsEditor = mPreferences.edit();
        // Get screen size
        Display display = ((Activity) getContext()).getWindowManager().getDefaultDisplay();
        DisplayMetrics outMetrics = new DisplayMetrics();
        display.getMetrics(outMetrics);
        float maxX = outMetrics.heightPixels;
        float maxY = outMetrics.widthPixels;
        // Height and width changes depending on orientation. Use the larger value for height.
        if (maxY > maxX) {
            float tmp = maxX;
            maxX = maxY;
            maxY = tmp;
        }
        Resources res = getResources();

        // Each value is a percent from max X/Y stored as an int. Have to bring that value down
        // to a decimal before multiplying by MAX X/Y.
        sPrefsEditor.putFloat(ButtonType.BUTTON_A + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_A_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_A + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_A_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_B + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_B_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_B + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_B_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_X + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_X_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_X + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_X_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_Y + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_Y_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_Y + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_Y_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_ZL + "-X", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_ZL_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_ZL + "-Y", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_ZL_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_ZR + "-X", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_ZR_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_ZR + "-Y", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_ZR_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.DPAD_UP + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_UP_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.DPAD_UP + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_UP_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_L + "-X", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_L_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_L + "-Y", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_L_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_R + "-X", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_R_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.TRIGGER_R + "-Y", (((float) res.getInteger(R.integer.SWITCH_TRIGGER_R_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_PLUS + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_PLUS_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_PLUS + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_PLUS_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_MINUS + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_MINUS_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_MINUS + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_MINUS_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_HOME + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_HOME_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_HOME + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_HOME_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.BUTTON_CAPTURE + "-X", (((float) res.getInteger(R.integer.SWITCH_BUTTON_CAPTURE_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.BUTTON_CAPTURE + "-Y", (((float) res.getInteger(R.integer.SWITCH_BUTTON_CAPTURE_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.STICK_R + "-X", (((float) res.getInteger(R.integer.SWITCH_STICK_R_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.STICK_R + "-Y", (((float) res.getInteger(R.integer.SWITCH_STICK_R_Y) / 1000) * maxY));
        sPrefsEditor.putFloat(ButtonType.STICK_L + "-X", (((float) res.getInteger(R.integer.SWITCH_STICK_L_X) / 1000) * maxX));
        sPrefsEditor.putFloat(ButtonType.STICK_L + "-Y", (((float) res.getInteger(R.integer.SWITCH_STICK_L_Y) / 1000) * maxY));

        // We want to commit right away, otherwise the overlay could load before this is saved.
        sPrefsEditor.commit();
    }

    public boolean isInEditMode() {
        return mIsInEditMode;
    }
}
