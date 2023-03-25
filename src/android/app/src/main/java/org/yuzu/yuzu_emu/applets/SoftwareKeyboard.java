// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.applets;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.DialogInterface;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.os.ResultReceiver;
import android.text.InputFilter;
import android.text.InputType;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.view.WindowInsets;
import android.view.inputmethod.InputMethodManager;
import android.widget.EditText;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.core.view.ViewCompat;
import androidx.fragment.app.DialogFragment;

import com.google.android.material.dialog.MaterialAlertDialogBuilder;

import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.activities.EmulationActivity;

import java.util.Objects;

public final class SoftwareKeyboard {
    /// Corresponds to Service::AM::Applets::SwkbdType
    private interface SwkbdType {
        int Normal = 0;
        int NumberPad = 1;
        int Qwerty = 2;
        int Unknown3 = 3;
        int Latin = 4;
        int SimplifiedChinese = 5;
        int TraditionalChinese = 6;
        int Korean = 7;
    };

    /// Corresponds to Service::AM::Applets::SwkbdPasswordMode
    private interface SwkbdPasswordMode {
        int Disabled = 0;
        int Enabled = 1;
    };

    /// Corresponds to Service::AM::Applets::SwkbdResult
    private interface SwkbdResult {
        int Ok = 0;
        int Cancel = 1;
    };

    public static class KeyboardConfig implements java.io.Serializable {
        public String ok_text;
        public String header_text;
        public String sub_text;
        public String guide_text;
        public String initial_text;
        public short left_optional_symbol_key;
        public short right_optional_symbol_key;
        public int max_text_length;
        public int min_text_length;
        public int initial_cursor_position;
        public int type;
        public int password_mode;
        public int text_draw_type;
        public int key_disable_flags;
        public boolean use_blur_background;
        public boolean enable_backspace_button;
        public boolean enable_return_button;
        public boolean disable_cancel_button;
    }

    /// Corresponds to Frontend::KeyboardData
    public static class KeyboardData {
        public int result;
        public String text;

        private KeyboardData(int result, String text) {
            this.result = result;
            this.text = text;
        }
    }

    public static class KeyboardDialogFragment extends DialogFragment {
        static KeyboardDialogFragment newInstance(KeyboardConfig config) {
            KeyboardDialogFragment frag = new KeyboardDialogFragment();
            Bundle args = new Bundle();
            args.putSerializable("config", config);
            frag.setArguments(args);
            return frag;
        }

        @NonNull
        @Override
        public Dialog onCreateDialog(Bundle savedInstanceState) {
            final Activity emulationActivity = getActivity();
            assert emulationActivity != null;

            FrameLayout.LayoutParams params = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            params.leftMargin = params.rightMargin =
                    YuzuApplication.getAppContext().getResources().getDimensionPixelSize(
                            R.dimen.dialog_margin);

            KeyboardConfig config = Objects.requireNonNull(
                    (KeyboardConfig) requireArguments().getSerializable("config"));

            // Set up the input
            EditText editText = new EditText(YuzuApplication.getAppContext());
            editText.setHint(config.initial_text);
            editText.setSingleLine(!config.enable_return_button);
            editText.setLayoutParams(params);
            editText.setFilters(new InputFilter[]{new InputFilter.LengthFilter(config.max_text_length)});

            // Handle input type
            int input_type = 0;
            switch (config.type)
            {
                case SwkbdType.Normal:
                case SwkbdType.Qwerty:
                case SwkbdType.Unknown3:
                case SwkbdType.Latin:
                case SwkbdType.SimplifiedChinese:
                case SwkbdType.TraditionalChinese:
                case SwkbdType.Korean:
                default:
                    input_type = InputType.TYPE_CLASS_TEXT;
                    if (config.password_mode == SwkbdPasswordMode.Enabled)
                    {
                        input_type |= InputType.TYPE_TEXT_VARIATION_PASSWORD;
                    }
                    break;
                case SwkbdType.NumberPad:
                    input_type = InputType.TYPE_CLASS_NUMBER;
                    if (config.password_mode == SwkbdPasswordMode.Enabled)
                    {
                        input_type |= InputType.TYPE_NUMBER_VARIATION_PASSWORD;
                    }
                    break;
            }

            // Apply input type
            editText.setInputType(input_type);

            FrameLayout container = new FrameLayout(emulationActivity);
            container.addView(editText);

            String headerText = config.header_text.isEmpty() ? emulationActivity.getString(R.string.software_keyboard) : config.header_text;
            String okText = config.header_text.isEmpty() ? emulationActivity.getString(android.R.string.ok) : config.ok_text;

            MaterialAlertDialogBuilder builder = new MaterialAlertDialogBuilder(emulationActivity)
                    .setTitle(headerText)
                    .setView(container);
            setCancelable(false);

            builder.setPositiveButton(okText, null);
            builder.setNegativeButton(emulationActivity.getString(android.R.string.cancel), null);

            final AlertDialog dialog = builder.create();
            dialog.create();
            if (dialog.getButton(DialogInterface.BUTTON_POSITIVE) != null) {
                dialog.getButton(DialogInterface.BUTTON_POSITIVE).setOnClickListener((view) -> {
                    data.result = SwkbdResult.Ok;
                    data.text = editText.getText().toString();
                    dialog.dismiss();

                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }
            if (dialog.getButton(DialogInterface.BUTTON_NEUTRAL) != null) {
                dialog.getButton(DialogInterface.BUTTON_NEUTRAL).setOnClickListener((view) -> {
                    data.result = SwkbdResult.Ok;
                    dialog.dismiss();
                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }
            if (dialog.getButton(DialogInterface.BUTTON_NEGATIVE) != null) {
                dialog.getButton(DialogInterface.BUTTON_NEGATIVE).setOnClickListener((view) -> {
                    data.result = SwkbdResult.Cancel;
                    dialog.dismiss();
                    synchronized (finishLock) {
                        finishLock.notifyAll();
                    }
                });
            }

            return dialog;
        }
    }

    private static KeyboardData data;
    private static final Object finishLock = new Object();

    private static void ExecuteNormalImpl(KeyboardConfig config) {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();

        data = new KeyboardData(SwkbdResult.Cancel, "");

        KeyboardDialogFragment fragment = KeyboardDialogFragment.newInstance(config);
        fragment.show(emulationActivity.getSupportFragmentManager(), "keyboard");
    }

    private static void ExecuteInlineImpl(KeyboardConfig config) {
        final EmulationActivity emulationActivity = NativeLibrary.sEmulationActivity.get();

        var overlayView = emulationActivity.findViewById(R.id.surface_input_overlay);
        InputMethodManager im = (InputMethodManager)overlayView.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
        im.showSoftInput(overlayView, InputMethodManager.SHOW_FORCED);

        // There isn't a good way to know that the IMM is dismissed, so poll every 500ms to submit inline keyboard result.
        final Handler handler = new Handler();
        final int delayMs = 500;
        handler.postDelayed(new Runnable() {
            public void run() {
                var insets = ViewCompat.getRootWindowInsets(overlayView);
                var isKeyboardVisible = insets.isVisible(WindowInsets.Type.ime());
                if (isKeyboardVisible) {
                    handler.postDelayed(this, delayMs);
                    return;
                }

                // No longer visible, submit the result.
                NativeLibrary.SubmitInlineKeyboardInput(android.view.KeyEvent.KEYCODE_ENTER);
            }
        }, delayMs);
    }

    public static KeyboardData ExecuteNormal(KeyboardConfig config) {
        NativeLibrary.sEmulationActivity.get().runOnUiThread(() -> ExecuteNormalImpl(config));

        synchronized (finishLock) {
            try {
                finishLock.wait();
            } catch (Exception ignored) {
            }
        }

        return data;
    }

    public static void ExecuteInline(KeyboardConfig config) {
        NativeLibrary.sEmulationActivity.get().runOnUiThread(() -> ExecuteInlineImpl(config));
    }

    public static void ShowError(String error) {
        NativeLibrary.displayAlertMsg(
                YuzuApplication.getAppContext().getResources().getString(R.string.software_keyboard),
                error, false);
    }
}
