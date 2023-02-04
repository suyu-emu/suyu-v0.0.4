package org.yuzu.yuzu_emu.activities;

import android.app.Activity;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.Handler;
import android.preference.PreferenceManager;
import android.view.InputDevice;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.widget.SeekBar;
import android.widget.TextView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AlertDialog;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.NotificationManagerCompat;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;

import org.yuzu.yuzu_emu.NativeLibrary;
import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.fragments.EmulationFragment;
import org.yuzu.yuzu_emu.fragments.MenuFragment;
import org.yuzu.yuzu_emu.utils.ControllerMappingHelper;
import org.yuzu.yuzu_emu.utils.ForegroundService;

import java.lang.annotation.Retention;

import static java.lang.annotation.RetentionPolicy.SOURCE;

public final class EmulationActivity extends AppCompatActivity {
    private static final String BACKSTACK_NAME_MENU = "menu";

    private static final String BACKSTACK_NAME_SUBMENU = "submenu";

    public static final String EXTRA_SELECTED_GAME = "SelectedGame";
    public static final String EXTRA_SELECTED_TITLE = "SelectedTitle";
    public static final int MENU_ACTION_EDIT_CONTROLS_PLACEMENT = 0;
    public static final int MENU_ACTION_TOGGLE_CONTROLS = 1;
    public static final int MENU_ACTION_ADJUST_SCALE = 2;
    public static final int MENU_ACTION_EXIT = 3;
    public static final int MENU_ACTION_SHOW_FPS = 4;
    public static final int MENU_ACTION_RESET_OVERLAY = 6;
    public static final int MENU_ACTION_SHOW_OVERLAY = 7;
    public static final int MENU_ACTION_OPEN_SETTINGS = 8;
    private static final int EMULATION_RUNNING_NOTIFICATION = 0x1000;
    private View mDecorView;
    private EmulationFragment mEmulationFragment;
    private SharedPreferences mPreferences;
    private ControllerMappingHelper mControllerMappingHelper;
    // TODO(bunnei): Disable notifications until we support app suspension.
//    private Intent foregroundService;
    private boolean activityRecreated;
    private String mSelectedTitle;
    private String mPath;

    private boolean mMenuVisible;

    public static void launch(FragmentActivity activity, String path, String title) {
        Intent launcher = new Intent(activity, EmulationActivity.class);

        launcher.putExtra(EXTRA_SELECTED_GAME, path);
        launcher.putExtra(EXTRA_SELECTED_TITLE, title);
        activity.startActivity(launcher);
    }

    public static void tryDismissRunningNotification(Activity activity) {
        // TODO(bunnei): Disable notifications until we support app suspension.
//        NotificationManagerCompat.from(activity).cancel(EMULATION_RUNNING_NOTIFICATION);
    }

    @Override
    protected void onDestroy() {
        // TODO(bunnei): Disable notifications until we support app suspension.
//        stopService(foregroundService);
        super.onDestroy();
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        if (savedInstanceState == null) {
            // Get params we were passed
            Intent gameToEmulate = getIntent();
            mPath = gameToEmulate.getStringExtra(EXTRA_SELECTED_GAME);
            mSelectedTitle = gameToEmulate.getStringExtra(EXTRA_SELECTED_TITLE);
            activityRecreated = false;
        } else {
            activityRecreated = true;
            restoreState(savedInstanceState);
        }

        mControllerMappingHelper = new ControllerMappingHelper();

        // Get a handle to the Window containing the UI.
        mDecorView = getWindow().getDecorView();
        mDecorView.setOnSystemUiVisibilityChangeListener(visibility ->
        {
            if ((visibility & View.SYSTEM_UI_FLAG_FULLSCREEN) == 0) {
                // Go back to immersive fullscreen mode in 3s
                Handler handler = new Handler(getMainLooper());
                handler.postDelayed(this::enableFullscreenImmersive, 3000 /* 3s */);
            }
        });
        // Set these options now so that the SurfaceView the game renders into is the right size.
        enableFullscreenImmersive();

        setTheme(R.style.YuzuEmulationBase);

        setContentView(R.layout.activity_emulation);

        // Find or create the EmulationFragment
        mEmulationFragment = (EmulationFragment) getSupportFragmentManager()
                .findFragmentById(R.id.frame_emulation_fragment);
        if (mEmulationFragment == null) {
            mEmulationFragment = EmulationFragment.newInstance(mPath);
            getSupportFragmentManager().beginTransaction()
                    .add(R.id.frame_emulation_fragment, mEmulationFragment)
                    .commit();
        }

        setTitle(mSelectedTitle);

        mPreferences = PreferenceManager.getDefaultSharedPreferences(this);

        // Start a foreground service to prevent the app from getting killed in the background
        // TODO(bunnei): Disable notifications until we support app suspension.
//        foregroundService = new Intent(EmulationActivity.this, ForegroundService.class);
//        startForegroundService(foregroundService);
    }

    @Override
    protected void onSaveInstanceState(@NonNull Bundle outState) {
        outState.putString(EXTRA_SELECTED_GAME, mPath);
        outState.putString(EXTRA_SELECTED_TITLE, mSelectedTitle);
        super.onSaveInstanceState(outState);
    }

    protected void restoreState(Bundle savedInstanceState) {
        mPath = savedInstanceState.getString(EXTRA_SELECTED_GAME);
        mSelectedTitle = savedInstanceState.getString(EXTRA_SELECTED_TITLE);

        // If an alert prompt was in progress when state was restored, retry displaying it
        NativeLibrary.retryDisplayAlertPrompt();
    }

    @Override
    public void onRestart() {
        super.onRestart();
    }

    @Override
    public void onBackPressed() {
        toggleMenu();
    }

    private void enableFullscreenImmersive() {
        // It would be nice to use IMMERSIVE_STICKY, but that doesn't show the toolbar.
        mDecorView.setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                        View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                        View.SYSTEM_UI_FLAG_FULLSCREEN |
                        View.SYSTEM_UI_FLAG_IMMERSIVE);
    }

    public void handleMenuAction(int action) {
        switch (action) {
            case MENU_ACTION_EXIT:
                mEmulationFragment.stopEmulation();
                finish();
                break;
        }
    }

    private void editControlsPlacement() {
        if (mEmulationFragment.isConfiguringControls()) {
            mEmulationFragment.stopConfiguringControls();
        } else {
            mEmulationFragment.startConfiguringControls();
        }
    }

    private void adjustScale() {
        LayoutInflater inflater = LayoutInflater.from(this);
        View view = inflater.inflate(R.layout.dialog_seekbar, null);

        final SeekBar seekbar = view.findViewById(R.id.seekbar);
        final TextView value = view.findViewById(R.id.text_value);
        final TextView units = view.findViewById(R.id.text_units);

        seekbar.setMax(150);
        seekbar.setProgress(mPreferences.getInt("controlScale", 50));
        seekbar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            public void onStartTrackingTouch(SeekBar seekBar) {
            }

            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                value.setText(String.valueOf(progress + 50));
            }

            public void onStopTrackingTouch(SeekBar seekBar) {
                setControlScale(seekbar.getProgress());
            }
        });

        value.setText(String.valueOf(seekbar.getProgress() + 50));
        units.setText("%");

        AlertDialog.Builder builder = new AlertDialog.Builder(this);
        builder.setTitle(R.string.emulation_control_scale);
        builder.setView(view);
        final int previousProgress = seekbar.getProgress();
        builder.setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> {
            setControlScale(previousProgress);
        });
        builder.setPositiveButton(android.R.string.ok, (dialogInterface, i) ->
        {
            setControlScale(seekbar.getProgress());
        });
        builder.setNeutralButton(R.string.slider_default, (dialogInterface, i) -> {
            setControlScale(50);
        });

        AlertDialog alertDialog = builder.create();
        alertDialog.show();
    }

    private void setControlScale(int scale) {
        SharedPreferences.Editor editor = mPreferences.edit();
        editor.putInt("controlScale", scale);
        editor.apply();
        mEmulationFragment.refreshInputOverlay();
    }

    private void resetOverlay() {
        new AlertDialog.Builder(this)
                .setTitle(getString(R.string.emulation_touch_overlay_reset))
                .setPositiveButton(android.R.string.yes, (dialogInterface, i) -> mEmulationFragment.resetInputOverlay())
                .setNegativeButton(android.R.string.cancel, (dialogInterface, i) -> {
                })
                .create()
                .show();
    }

    private static boolean areCoordinatesOutside(@Nullable View view, float x, float y)
    {
        if (view == null)
        {
            return true;
        }

        Rect viewBounds = new Rect();
        view.getGlobalVisibleRect(viewBounds);
        return !viewBounds.contains(Math.round(x), Math.round(y));
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent event)
    {
        if (event.getActionMasked() == MotionEvent.ACTION_DOWN)
        {
            boolean anyMenuClosed = false;

            Fragment submenu = getSupportFragmentManager().findFragmentById(R.id.frame_submenu);
            if (submenu != null && areCoordinatesOutside(submenu.getView(), event.getX(), event.getY()))
            {
                closeSubmenu();
                submenu = null;
                anyMenuClosed = true;
            }

            if (submenu == null)
            {
                Fragment menu = getSupportFragmentManager().findFragmentById(R.id.frame_menu);
                if (menu != null && areCoordinatesOutside(menu.getView(), event.getX(), event.getY()))
                {
                    closeMenu();
                    anyMenuClosed = true;
                }
            }

            if (anyMenuClosed)
            {
                return true;
            }
        }

        return super.dispatchTouchEvent(event);
    }

    public boolean isActivityRecreated() {
        return activityRecreated;
    }

    @Retention(SOURCE)
    @IntDef({MENU_ACTION_EDIT_CONTROLS_PLACEMENT, MENU_ACTION_TOGGLE_CONTROLS, MENU_ACTION_ADJUST_SCALE,
            MENU_ACTION_EXIT, MENU_ACTION_SHOW_FPS, MENU_ACTION_RESET_OVERLAY, MENU_ACTION_SHOW_OVERLAY, MENU_ACTION_OPEN_SETTINGS})
    public @interface MenuAction {
    }

    private boolean closeSubmenu()
    {
        return getSupportFragmentManager().popBackStackImmediate(BACKSTACK_NAME_SUBMENU,
                FragmentManager.POP_BACK_STACK_INCLUSIVE);
    }

    private boolean closeMenu()
    {
        mMenuVisible = false;
        return getSupportFragmentManager().popBackStackImmediate(BACKSTACK_NAME_MENU,
                FragmentManager.POP_BACK_STACK_INCLUSIVE);
    }

    private void toggleMenu()
    {
        if (!closeMenu()) {
            // Removing the menu failed, so that means it wasn't visible. Add it.
            Fragment fragment = MenuFragment.newInstance();
            getSupportFragmentManager().beginTransaction()
                    .setCustomAnimations(
                            R.animator.menu_slide_in_from_start,
                            R.animator.menu_slide_out_to_start,
                            R.animator.menu_slide_in_from_start,
                            R.animator.menu_slide_out_to_start)
                    .add(R.id.frame_menu, fragment)
                    .addToBackStack(BACKSTACK_NAME_MENU)
                    .commit();
            mMenuVisible = true;
        }
    }

}
