package org.yuzu.yuzu_emu.utils;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import androidx.appcompat.app.AlertDialog;

import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.ui.main.MainActivity;
import org.yuzu.yuzu_emu.ui.main.MainPresenter;

public final class StartupHandler {
    private static SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.getAppContext());

    private static void handleStartupPromptDismiss(MainActivity parent) {
        parent.launchFileListActivity(MainPresenter.REQUEST_ADD_DIRECTORY);
    }

    private static void markFirstBoot() {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putBoolean("FirstApplicationLaunch", false);
        editor.apply();
    }

    public static void handleInit(MainActivity parent) {
        if (mPreferences.getBoolean("FirstApplicationLaunch", true)) {
            markFirstBoot();

            // Prompt user with standard first boot disclaimer
            new AlertDialog.Builder(parent)
                    .setTitle(R.string.app_name)
                    .setIcon(R.mipmap.ic_launcher)
                    .setMessage(parent.getResources().getString(R.string.app_disclaimer))
                    .setPositiveButton(android.R.string.ok, null)
                    .setOnDismissListener(dialogInterface -> handleStartupPromptDismiss(parent))
                    .show();
        }
    }
}
