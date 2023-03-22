// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import androidx.preference.PreferenceManager
import android.text.Html
import android.text.method.LinkMovementMethod
import android.view.View
import android.widget.TextView
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.ui.main.MainActivity
import org.yuzu.yuzu_emu.ui.main.MainPresenter

object StartupHandler {
    private val preferences =
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    private fun handleStartupPromptDismiss(parent: MainActivity) {
        parent.launchFileListActivity(MainPresenter.REQUEST_INSTALL_KEYS)
    }

    private fun markFirstBoot() {
        preferences.edit()
            .putBoolean(Settings.PREF_FIRST_APP_LAUNCH, false)
            .apply()
    }

    fun handleInit(parent: MainActivity) {
        if (preferences.getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)) {
            markFirstBoot()
            val alert = MaterialAlertDialogBuilder(parent)
                .setMessage(Html.fromHtml(parent.resources.getString(R.string.app_disclaimer)))
                .setTitle(R.string.app_name)
                .setIcon(R.drawable.ic_launcher)
                .setPositiveButton(android.R.string.ok, null)
                .setOnDismissListener {
                    handleStartupPromptDismiss(parent)
                }
                .show()
            (alert.findViewById<View>(android.R.id.message) as TextView?)!!.movementMethod =
                LinkMovementMethod.getInstance()
        }
    }
}
