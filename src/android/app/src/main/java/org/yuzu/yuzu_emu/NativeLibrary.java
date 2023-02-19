/*
 * Copyright 2013 Dolphin Emulator Project
 * Licensed under GPLv2+
 * Refer to the license.txt file included.
 */

package org.yuzu.yuzu_emu;

import android.app.Activity;
import android.app.Dialog;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.Bundle;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.view.Surface;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.ContextCompat;
import androidx.fragment.app.DialogFragment;

import org.yuzu.yuzu_emu.activities.EmulationActivity;
import org.yuzu.yuzu_emu.utils.DocumentsTree;
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings;
import org.yuzu.yuzu_emu.utils.FileUtil;
import org.yuzu.yuzu_emu.utils.Log;

import java.lang.ref.WeakReference;
import java.util.Objects;

import static android.Manifest.permission.CAMERA;
import static android.Manifest.permission.RECORD_AUDIO;

/**
 * Class which contains methods that interact
 * with the native side of the Citra code.
 */
public final class NativeLibrary {
    /**
     * Default controller id for each device
     */
    public static final int Player1Device = 0;
    public static final int Player2Device = 1;
    public static final int Player3Device = 2;
    public static final int Player4Device = 3;
    public static final int Player5Device = 4;
    public static final int Player6Device = 5;
    public static final int Player7Device = 6;
    public static final int Player8Device = 7;
    public static final int ConsoleDevice = 8;
    public static WeakReference<EmulationActivity> sEmulationActivity = new WeakReference<>(null);

    private static boolean alertResult = false;
    private static String alertPromptResult = "";
    private static int alertPromptButton = 0;
    private static final Object alertPromptLock = new Object();
    private static boolean alertPromptInProgress = false;
    private static String alertPromptCaption = "";
    private static int alertPromptButtonConfig = 0;
    private static EditText alertPromptEditText = null;

    static {
        try {
            System.loadLibrary("yuzu-android");
        } catch (UnsatisfiedLinkError ex) {
            Log.error("[NativeLibrary] " + ex.toString());
        }
    }

    private NativeLibrary() {
        // Disallows instantiation.
    }

    public static int openContentUri(String path, String openmode) {
        if (DocumentsTree.isNativePath(path)) {
            return YuzuApplication.documentsTree.openContentUri(path, openmode);
        }
        return FileUtil.openContentUri(YuzuApplication.getAppContext(), path, openmode);
    }

    public static long getSize(String path) {
        if (DocumentsTree.isNativePath(path)) {
            return YuzuApplication.documentsTree.getFileSize(path);
        }
        return FileUtil.getFileSize(YuzuApplication.getAppContext(), path);
    }

    /**
     * Handles button press events for a gamepad.
     *
     * @param Device The input descriptor of the gamepad.
     * @param Button Key code identifying which button was pressed.
     * @param Action Mask identifying which action is happening (button pressed down, or button released).
     * @return If we handled the button press.
     */
    public static native boolean onGamePadButtonEvent(int Device, int Button, int Action);

    /**
     * Handles joystick movement events.
     *
     * @param Device The device ID of the gamepad.
     * @param Axis   The axis ID
     * @param x_axis The value of the x-axis represented by the given ID.
     * @param y_axis The value of the y-axis represented by the given ID.
     */
    public static native boolean onGamePadJoystickEvent(int Device, int Axis, float x_axis, float y_axis);

    /**
     * Handles motion events.
     *
     * @param delta_timestamp         The finger id corresponding to this event
     * @param gyro_x,gyro_y,gyro_z    The value of the accelerometer sensor.
     * @param accel_x,accel_y,accel_z The value of the y-axis
     */

    public static native boolean onGamePadMotionEvent(int Device, long delta_timestamp, float gyro_x, float gyro_y,
                                                      float gyro_z, float accel_x, float accel_y, float accel_z);

    /**
     * Handles touch press events.
     *
     * @param finger_id The finger id corresponding to this event
     * @param x_axis    The value of the x-axis.
     * @param y_axis    The value of the y-axis.
     */
    public static native void onTouchPressed(int finger_id, float x_axis, float y_axis);

    /**
     * Handles touch movement.
     *
     * @param x_axis The value of the instantaneous x-axis.
     * @param y_axis The value of the instantaneous y-axis.
     */
    public static native void onTouchMoved(int finger_id, float x_axis, float y_axis);

    /**
     * Handles touch release events.
     *
     * @param finger_id The finger id corresponding to this event
     */
    public static native void onTouchReleased(int finger_id);

    public static native void ReloadSettings();

    public static native String GetUserSetting(String gameID, String Section, String Key);

    public static native void SetUserSetting(String gameID, String Section, String Key, String Value);

    public static native void InitGameIni(String gameID);

    /**
     * Gets the embedded icon within the given ROM.
     *
     * @param filename the file path to the ROM.
     * @return a byte array containing the JPEG data for the icon.
     */
    public static native byte[] GetIcon(String filename);

    /**
     * Gets the embedded title of the given ISO/ROM.
     *
     * @param filename The file path to the ISO/ROM.
     * @return the embedded title of the ISO/ROM.
     */
    public static native String GetTitle(String filename);

    public static native String GetDescription(String filename);

    public static native String GetGameId(String filename);

    public static native String GetRegions(String filename);

    public static native String GetCompany(String filename);

    public static native String GetGitRevision();

    public static native void SetAppDirectory(String directory);

    public static native void SetGpuDriverParameters(String hookLibDir, String customDriverDir, String customDriverName, String fileRedirectDir);

    public static native boolean ReloadKeys();

    // Create the config.ini file.
    public static native void CreateConfigFile();

    public static native int DefaultCPUCore();

    /**
     * Begins emulation.
     */
    public static native void Run(String path);

    /**
     * Begins emulation from the specified savestate.
     */
    public static native void Run(String path, String savestatePath, boolean deleteSavestate);

    // Surface Handling
    public static native void SurfaceChanged(Surface surf);

    public static native void SurfaceDestroyed();

    public static native void DoFrame();

    /**
     * Unpauses emulation from a paused state.
     */
    public static native void UnPauseEmulation();

    /**
     * Pauses emulation.
     */
    public static native void PauseEmulation();

    /**
     * Stops emulation.
     */
    public static native void StopEmulation();

    /**
     * Resets the in-memory ROM metadata cache.
     */
    public static native void ResetRomMetadata();

    /**
     * Returns true if emulation is running (or is paused).
     */
    public static native boolean IsRunning();

    /**
     * Returns the performance stats for the current game
     **/
    public static native double[] GetPerfStats();

    /**
     * Notifies the core emulation that the orientation has changed.
     */
    public static native void NotifyOrientationChange(int layout_option, int rotation);

    public enum CoreError {
        ErrorSystemFiles,
        ErrorSavestate,
        ErrorUnknown,
    }

    private static boolean coreErrorAlertResult = false;
    private static final Object coreErrorAlertLock = new Object();

    public static class CoreErrorDialogFragment extends DialogFragment {
        static CoreErrorDialogFragment newInstance(String title, String message) {
            CoreErrorDialogFragment frag = new CoreErrorDialogFragment();
            Bundle args = new Bundle();
            args.putString("title", title);
            args.putString("message", message);
            frag.setArguments(args);
            return frag;
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Activity emulationActivity = Objects.requireNonNull(getActivity());

            final String title = Objects.requireNonNull(Objects.requireNonNull(getArguments()).getString("title"));
            final String message = Objects.requireNonNull(Objects.requireNonNull(getArguments()).getString("message"));

            return new AlertDialog.Builder(emulationActivity)
                    .setTitle(title)
                    .setMessage(message)
                    .setPositiveButton(R.string.continue_button, (dialog, which) -> {
                        coreErrorAlertResult = true;
                        synchronized (coreErrorAlertLock) {
                            coreErrorAlertLock.notify();
                        }
                    })
                    .setNegativeButton(R.string.abort_button, (dialog, which) -> {
                        coreErrorAlertResult = false;
                        synchronized (coreErrorAlertLock) {
                            coreErrorAlertLock.notify();
                        }
                    }).setOnDismissListener(dialog -> {
                        coreErrorAlertResult = true;
                        synchronized (coreErrorAlertLock) {
                            coreErrorAlertLock.notify();
                        }
                    }).create();
        }
    }

    private static void OnCoreErrorImpl(String title, String message) {
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present");
            return;
        }

        CoreErrorDialogFragment fragment = CoreErrorDialogFragment.newInstance(title, message);
        fragment.show(emulationActivity.getSupportFragmentManager(), "coreError");
    }

    /**
     * Handles a core error.
     *
     * @return true: continue; false: abort
     */
    public static boolean OnCoreError(CoreError error, String details) {
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present");
            return false;
        }

        String title, message;
        switch (error) {
            case ErrorSystemFiles: {
                title = emulationActivity.getString(R.string.system_archive_not_found);
                message = emulationActivity.getString(R.string.system_archive_not_found_message, details.isEmpty() ? emulationActivity.getString(R.string.system_archive_general) : details);
                break;
            }
            case ErrorSavestate: {
                title = emulationActivity.getString(R.string.save_load_error);
                message = details;
                break;
            }
            case ErrorUnknown: {
                title = emulationActivity.getString(R.string.fatal_error);
                message = emulationActivity.getString(R.string.fatal_error_message);
                break;
            }
            default: {
                return true;
            }
        }

        // Show the AlertDialog on the main thread.
        emulationActivity.runOnUiThread(() -> OnCoreErrorImpl(title, message));

        // Wait for the lock to notify that it is complete.
        synchronized (coreErrorAlertLock) {
            try {
                coreErrorAlertLock.wait();
            } catch (Exception ignored) {
            }
        }

        return coreErrorAlertResult;
    }

    public static boolean isPortraitMode() {
        return YuzuApplication.getAppContext().getResources().getConfiguration().orientation ==
                Configuration.ORIENTATION_PORTRAIT;
    }

    public static int landscapeScreenLayout() {
        return EmulationMenuSettings.getLandscapeScreenLayout();
    }

    public static boolean displayAlertMsg(final String caption, final String text,
                                          final boolean yesNo) {
        Log.error("[NativeLibrary] Alert: " + text);
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        boolean result = false;
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null, can't do panic alert.");
        } else {
            // Create object used for waiting.
            final Object lock = new Object();
            AlertDialog.Builder builder = new AlertDialog.Builder(emulationActivity)
                    .setTitle(caption)
                    .setMessage(text);

            // If not yes/no dialog just have one button that dismisses modal,
            // otherwise have a yes and no button that sets alertResult accordingly.
            if (!yesNo) {
                builder
                        .setCancelable(false)
                        .setPositiveButton(android.R.string.ok, (dialog, whichButton) ->
                        {
                            dialog.dismiss();
                            synchronized (lock) {
                                lock.notify();
                            }
                        });
            } else {
                alertResult = false;

                builder
                        .setPositiveButton(android.R.string.yes, (dialog, whichButton) ->
                        {
                            alertResult = true;
                            dialog.dismiss();
                            synchronized (lock) {
                                lock.notify();
                            }
                        })
                        .setNegativeButton(android.R.string.no, (dialog, whichButton) ->
                        {
                            alertResult = false;
                            dialog.dismiss();
                            synchronized (lock) {
                                lock.notify();
                            }
                        });
            }

            // Show the AlertDialog on the main thread.
            emulationActivity.runOnUiThread(builder::show);

            // Wait for the lock to notify that it is complete.
            synchronized (lock) {
                try {
                    lock.wait();
                } catch (Exception e) {
                }
            }

            if (yesNo)
                result = alertResult;
        }
        return result;
    }

    public static void retryDisplayAlertPrompt() {
        if (!alertPromptInProgress) {
            return;
        }
        displayAlertPromptImpl(alertPromptCaption, alertPromptEditText.getText().toString(), alertPromptButtonConfig).show();
    }

    public static String displayAlertPrompt(String caption, String text, int buttonConfig) {
        alertPromptCaption = caption;
        alertPromptButtonConfig = buttonConfig;
        alertPromptInProgress = true;

        // Show the AlertDialog on the main thread
        sEmulationActivity.get().runOnUiThread(() -> displayAlertPromptImpl(alertPromptCaption, text, alertPromptButtonConfig).show());

        // Wait for the lock to notify that it is complete
        synchronized (alertPromptLock) {
            try {
                alertPromptLock.wait();
            } catch (Exception e) {
            }
        }
        alertPromptInProgress = false;

        return alertPromptResult;
    }

    public static AlertDialog.Builder displayAlertPromptImpl(String caption, String text, int buttonConfig) {
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        alertPromptResult = "";
        alertPromptButton = 0;

        FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        params.leftMargin = params.rightMargin = YuzuApplication.getAppContext().getResources().getDimensionPixelSize(R.dimen.dialog_margin);

        // Set up the input
        alertPromptEditText = new EditText(YuzuApplication.getAppContext());
        alertPromptEditText.setText(text);
        alertPromptEditText.setSingleLine();
        alertPromptEditText.setLayoutParams(params);

        FrameLayout container = new FrameLayout(emulationActivity);
        container.addView(alertPromptEditText);

        AlertDialog.Builder builder = new AlertDialog.Builder(emulationActivity)
                .setTitle(caption)
                .setView(container)
                .setPositiveButton(android.R.string.ok, (dialogInterface, i) ->
                {
                    alertPromptButton = buttonConfig;
                    alertPromptResult = alertPromptEditText.getText().toString();
                    synchronized (alertPromptLock) {
                        alertPromptLock.notifyAll();
                    }
                })
                .setOnDismissListener(dialogInterface ->
                {
                    alertPromptResult = "";
                    synchronized (alertPromptLock) {
                        alertPromptLock.notifyAll();
                    }
                });

        if (buttonConfig > 0) {
            builder.setNegativeButton(android.R.string.cancel, (dialogInterface, i) ->
            {
                alertPromptResult = "";
                synchronized (alertPromptLock) {
                    alertPromptLock.notifyAll();
                }
            });
        }

        return builder;
    }

    public static int alertPromptButton() {
        return alertPromptButton;
    }

    public static void exitEmulationActivity(int resultCode) {
        final int Success = 0;
        final int ErrorNotInitialized = 1;
        final int ErrorGetLoader = 2;
        final int ErrorSystemMode = 3;
        final int ErrorLoader = 4;
        final int ErrorLoader_ErrorEncrypted = 5;
        final int ErrorLoader_ErrorInvalidFormat = 6;
        final int ErrorSystemFiles = 7;
        final int ErrorVideoCore = 8;
        final int ErrorVideoCore_ErrorGenericDrivers = 9;
        final int ErrorVideoCore_ErrorBelowGL33 = 10;
        final int ShutdownRequested = 11;
        final int ErrorUnknown = 12;

        int captionId = R.string.loader_error_invalid_format;
        if (resultCode == ErrorLoader_ErrorEncrypted) {
            captionId = R.string.loader_error_encrypted;
        }

        String formatedText = "Please follow the guides to redump your <a href=\"https://yuzu-emu.org/help/quickstart/#dumping-cartridge-games\">game cartidges</a> or <a href=\"https://yuzu-emu.org/help/quickstart/#dumping-installed-titles-eshop\">installed titles</a>.";
        if (!ReloadKeys()) {
            formatedText = "Please ensure your <a href=\"https://yuzu-emu.org/help/quickstart/#dumping-prodkeys-and-titlekeys\">prod.keys</a> file is installed so that games can be decrypted.";
        }
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        if (emulationActivity == null) {
            Log.warning("[NativeLibrary] EmulationActivity is null, can't exit.");
            return;
        }

        AlertDialog.Builder builder = new AlertDialog.Builder(emulationActivity)
                .setTitle(captionId)
                .setMessage(Html.fromHtml(formatedText, Html.FROM_HTML_MODE_LEGACY))
                .setPositiveButton(android.R.string.ok, (dialog, whichButton) -> emulationActivity.finish())
                .setOnDismissListener(dialogInterface -> emulationActivity.finish());
        emulationActivity.runOnUiThread(() -> {
            AlertDialog alert = builder.create();
            alert.show();
            ((TextView) alert.findViewById(android.R.id.message)).setMovementMethod(LinkMovementMethod.getInstance());
        });
    }

    public static void setEmulationActivity(EmulationActivity emulationActivity) {
        Log.verbose("[NativeLibrary] Registering EmulationActivity.");
        sEmulationActivity = new WeakReference<>(emulationActivity);
    }

    public static void clearEmulationActivity() {
        Log.verbose("[NativeLibrary] Unregistering EmulationActivity.");

        sEmulationActivity.clear();
    }

    private static final Object cameraPermissionLock = new Object();
    private static boolean cameraPermissionGranted = false;
    public static final int REQUEST_CODE_NATIVE_CAMERA = 800;

    public static boolean RequestCameraPermission() {
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present");
            return false;
        }
        if (ContextCompat.checkSelfPermission(emulationActivity, CAMERA) == PackageManager.PERMISSION_GRANTED) {
            // Permission already granted
            return true;
        }
        emulationActivity.requestPermissions(new String[]{CAMERA}, REQUEST_CODE_NATIVE_CAMERA);

        // Wait until result is returned
        synchronized (cameraPermissionLock) {
            try {
                cameraPermissionLock.wait();
            } catch (InterruptedException ignored) {
            }
        }
        return cameraPermissionGranted;
    }

    public static void CameraPermissionResult(boolean granted) {
        cameraPermissionGranted = granted;
        synchronized (cameraPermissionLock) {
            cameraPermissionLock.notify();
        }
    }

    private static final Object micPermissionLock = new Object();
    private static boolean micPermissionGranted = false;
    public static final int REQUEST_CODE_NATIVE_MIC = 900;

    public static boolean RequestMicPermission() {
        final EmulationActivity emulationActivity = sEmulationActivity.get();
        if (emulationActivity == null) {
            Log.error("[NativeLibrary] EmulationActivity not present");
            return false;
        }
        if (ContextCompat.checkSelfPermission(emulationActivity, RECORD_AUDIO) == PackageManager.PERMISSION_GRANTED) {
            // Permission already granted
            return true;
        }
        emulationActivity.requestPermissions(new String[]{RECORD_AUDIO}, REQUEST_CODE_NATIVE_MIC);

        // Wait until result is returned
        synchronized (micPermissionLock) {
            try {
                micPermissionLock.wait();
            } catch (InterruptedException ignored) {
            }
        }
        return micPermissionGranted;
    }

    public static void MicPermissionResult(boolean granted) {
        micPermissionGranted = granted;
        synchronized (micPermissionLock) {
            micPermissionLock.notify();
        }
    }

    /**
     * Logs the Citra version, Android version and, CPU.
     */
    public static native void LogDeviceInfo();

    /**
     * Button type for use in onTouchEvent
     */
    public static final class ButtonType {
        public static final int BUTTON_A = 0;
        public static final int BUTTON_B = 1;
        public static final int BUTTON_X = 2;
        public static final int BUTTON_Y = 3;
        public static final int STICK_L = 4;
        public static final int STICK_R = 5;
        public static final int TRIGGER_L = 6;
        public static final int TRIGGER_R = 7;
        public static final int TRIGGER_ZL = 8;
        public static final int TRIGGER_ZR = 9;
        public static final int BUTTON_PLUS = 10;
        public static final int BUTTON_MINUS = 11;
        public static final int DPAD_LEFT = 12;
        public static final int DPAD_UP = 13;
        public static final int DPAD_RIGHT = 14;
        public static final int DPAD_DOWN = 15;
        public static final int BUTTON_SL = 16;
        public static final int BUTTON_SR = 17;
        public static final int BUTTON_HOME = 18;
        public static final int BUTTON_CAPTURE = 19;
    }

    /**
     * Stick type for use in onTouchEvent
     */
    public static final class StickType {
        public static final int STICK_L = 0;
        public static final int STICK_R = 1;
    }

    /**
     * Button states
     */
    public static final class ButtonState {
        public static final int RELEASED = 0;
        public static final int PRESSED = 1;
    }
}
