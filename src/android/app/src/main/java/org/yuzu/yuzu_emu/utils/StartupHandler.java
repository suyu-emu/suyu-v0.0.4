package org.yuzu.yuzu_emu.utils;

import android.content.SharedPreferences;
import android.preference.PreferenceManager;
import android.text.Html;
import android.text.method.LinkMovementMethod;
import android.widget.TextView;

import androidx.appcompat.app.AlertDialog;

import org.yuzu.yuzu_emu.R;
import org.yuzu.yuzu_emu.YuzuApplication;
import org.yuzu.yuzu_emu.ui.main.MainActivity;
import org.yuzu.yuzu_emu.ui.main.MainPresenter;

public final class StartupHandler {
    private static SharedPreferences mPreferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.getAppContext());

    private static void handleStartupPromptDismiss(MainActivity parent) {
        parent.launchFileListActivity(MainPresenter.REQUEST_INSTALL_KEYS);
    }

    private static void markFirstBoot() {
        final SharedPreferences.Editor editor = mPreferences.edit();
        editor.putBoolean("FirstApplicationLaunch", false);
        editor.apply();
    }

    public static void handleInit(MainActivity parent) {
        if (mPreferences.getBoolean("FirstApplicationLaunch", true)) {
            markFirstBoot();

            AlertDialog.Builder builder = new AlertDialog.Builder(parent);
            builder.setMessage(Html.fromHtml(parent.getResources().getString(R.string.app_disclaimer)));
            builder.setTitle(R.string.app_name);
            builder.setIcon(R.mipmap.ic_launcher);
            builder.setPositiveButton(android.R.string.ok, null);
            builder.setOnDismissListener(dialogInterface -> handleStartupPromptDismiss(parent));

            AlertDialog alert = builder.create();
            alert.show();
            ((TextView) alert.findViewById(android.R.id.message)).setMovementMethod(LinkMovementMethod.getInstance());
        }
    }
}
