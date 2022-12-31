package org.yuzu.yuzu_emu.fragments;

import android.content.pm.PackageManager;
import android.graphics.Rect;
import android.os.Bundle;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.Fragment;

import com.google.android.material.color.MaterialColors;
import com.google.android.material.elevation.ElevationOverlayProvider;

import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.activities.EmulationActivity;


public final class MenuFragment extends Fragment implements View.OnClickListener
{
    private static final String KEY_TITLE = "title";
    private static final String KEY_WII = "wii";
    private static SparseIntArray buttonsActionsMap = new SparseIntArray();

    private int mCutInset = 0;

    static
    {
        buttonsActionsMap.append(R.id.menu_exit, EmulationActivity.MENU_ACTION_EXIT);
    }

    public static MenuFragment newInstance()
    {
        MenuFragment fragment = new MenuFragment();

        Bundle arguments = new Bundle();
        fragment.setArguments(arguments);

        return fragment;
    }

    // This is primarily intended to account for any navigation bar at the bottom of the screen
    private int getBottomPaddingRequired()
    {
        Rect visibleFrame = new Rect();
        requireActivity().getWindow().getDecorView().getWindowVisibleDisplayFrame(visibleFrame);
        return visibleFrame.bottom - visibleFrame.top - getResources().getDisplayMetrics().heightPixels;
    }

    @NonNull
    @Override
    public View onCreateView(@NonNull LayoutInflater inflater, ViewGroup container,
                             Bundle savedInstanceState)
    {
        View rootView = inflater.inflate(R.layout.fragment_ingame_menu, container, false);

        LinearLayout options = rootView.findViewById(R.id.layout_options);

//        mPauseEmulation = options.findViewById(R.id.menu_pause_emulation);
//        mUnpauseEmulation = options.findViewById(R.id.menu_unpause_emulation);
//
//        updatePauseUnpauseVisibility();
//
//        if (!requireActivity().getPackageManager().hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN))
//        {
//            options.findViewById(R.id.menu_overlay_controls).setVisibility(View.GONE);
//        }
//
//        if (!getArguments().getBoolean(KEY_WII, true))
//        {
//            options.findViewById(R.id.menu_refresh_wiimotes).setVisibility(View.GONE);
//        }

        int bottomPaddingRequired = getBottomPaddingRequired();

        // Provide a safe zone between the navigation bar and Exit Emulation to avoid accidental touches
        float density = getResources().getDisplayMetrics().density;
        if (bottomPaddingRequired >= 32 * density)
        {
            bottomPaddingRequired += 32 * density;
        }

        if (bottomPaddingRequired > rootView.getPaddingBottom())
        {
            rootView.setPadding(rootView.getPaddingLeft(), rootView.getPaddingTop(),
                    rootView.getPaddingRight(), bottomPaddingRequired);
        }

        for (int childIndex = 0; childIndex < options.getChildCount(); childIndex++)
        {
            Button button = (Button) options.getChildAt(childIndex);

            button.setOnClickListener(this);
        }

        rootView.findViewById(R.id.menu_exit).setOnClickListener(this);

//        mTitleText = rootView.findViewById(R.id.text_game_title);
//        String title = getArguments().getString(KEY_TITLE, null);
//        if (title != null)
//        {
//            mTitleText.setText(title);
//        }

        if (getResources().getConfiguration().getLayoutDirection() == View.LAYOUT_DIRECTION_LTR)
        {
//            rootView.post(() -> NativeLibrary.SetObscuredPixelsLeft(rootView.getWidth()));
        }

        return rootView;
    }

    @Override
    public void onClick(View button)
    {
        int action = buttonsActionsMap.get(button.getId());
        EmulationActivity activity = (EmulationActivity) requireActivity();
        activity.handleMenuAction(action);
    }
}
