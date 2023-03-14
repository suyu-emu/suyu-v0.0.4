// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Bundle
import android.view.Menu
import android.widget.Toast
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.localbroadcastmanager.content.LocalBroadcastManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.progressindicator.LinearProgressIndicator
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.ui.SettingsFragment.Companion.newInstance
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.DirectoryStateReceiver
import org.yuzu.yuzu_emu.utils.EmulationMenuSettings
import org.yuzu.yuzu_emu.utils.ThemeHelper

class SettingsActivity : AppCompatActivity(), SettingsActivityView {
    private val presenter = SettingsActivityPresenter(this)
    private var dialog: AlertDialog? = null

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_settings)
        val launcher = intent
        val gameID = launcher.getStringExtra(ARG_GAME_ID)
        val menuTag = launcher.getStringExtra(ARG_MENU_TAG)
        presenter.onCreate(savedInstanceState, menuTag!!, gameID!!)

        // Show "Back" button in the action bar for navigation
        setSupportActionBar(findViewById(R.id.toolbar_settings))
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)
    }

    override fun onSupportNavigateUp(): Boolean {
        onBackPressed()
        return true
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        val inflater = menuInflater
        inflater.inflate(R.menu.menu_settings, menu)
        return true
    }

    override fun onSaveInstanceState(outState: Bundle) {
        // Critical: If super method is not called, rotations will be busted.
        super.onSaveInstanceState(outState)
        presenter.saveState(outState)
    }

    override fun onStart() {
        super.onStart()
        presenter.onStart()
    }

    /**
     * If this is called, the user has left the settings screen (potentially through the
     * home button) and will expect their changes to be persisted. So we kick off an
     * IntentService which will do so on a background thread.
     */
    override fun onStop() {
        super.onStop()
        presenter.onStop(isFinishing)

        // Update framebuffer layout when closing the settings
        NativeLibrary.NotifyOrientationChange(
            EmulationMenuSettings.landscapeScreenLayout,
            windowManager.defaultDisplay.rotation
        )
    }

    override fun showSettingsFragment(menuTag: String, addToStack: Boolean, gameId: String) {
        if (!addToStack && fragment != null) {
            return
        }
        val transaction = supportFragmentManager.beginTransaction()
        if (addToStack) {
            if (areSystemAnimationsEnabled()) {
                transaction.setCustomAnimations(
                    R.animator.settings_enter,
                    R.animator.settings_exit,
                    R.animator.settings_pop_enter,
                    R.animator.setttings_pop_exit
                )
            }
            transaction.addToBackStack(null)
        }
        transaction.replace(R.id.frame_content, newInstance(menuTag, gameId), FRAGMENT_TAG)
        transaction.commit()
    }

    private fun areSystemAnimationsEnabled(): Boolean {
        val duration = android.provider.Settings.Global.getFloat(
            contentResolver,
            android.provider.Settings.Global.ANIMATOR_DURATION_SCALE, 1f
        )
        val transition = android.provider.Settings.Global.getFloat(
            contentResolver,
            android.provider.Settings.Global.TRANSITION_ANIMATION_SCALE, 1f
        )
        return duration != 0f && transition != 0f
    }

    override fun startDirectoryInitializationService(
        receiver: DirectoryStateReceiver?,
        filter: IntentFilter
    ) {
        LocalBroadcastManager.getInstance(this).registerReceiver(
            receiver!!,
            filter
        )
        DirectoryInitialization.start(this)
    }

    override fun stopListeningToDirectoryInitializationService(receiver: DirectoryStateReceiver) {
        LocalBroadcastManager.getInstance(this).unregisterReceiver(receiver)
    }

    override fun showLoading() {
        if (dialog == null) {
            val root = layoutInflater.inflate(R.layout.dialog_progress_bar, null)
            val progressBar = root.findViewById<LinearProgressIndicator>(R.id.progress_bar)
            progressBar.isIndeterminate = true

            dialog = MaterialAlertDialogBuilder(this)
                .setTitle(R.string.load_settings)
                .setView(root)
                .setCancelable(false)
                .create()
        }
        dialog!!.show()
    }

    override fun hideLoading() {
        dialog!!.dismiss()
    }

    override fun showExternalStorageNotMountedHint() {
        Toast.makeText(
            this,
            R.string.external_storage_not_mounted,
            Toast.LENGTH_SHORT
        ).show()
    }

    override var settings: Settings?
        get() = presenter.settings
        set(settings) {
            presenter.settings = settings
        }

    override fun onSettingsFileLoaded(settings: Settings?) {
        val fragment: SettingsFragmentView? = fragment
        fragment?.onSettingsFileLoaded(settings)
    }

    override fun onSettingsFileNotFound() {
        val fragment: SettingsFragmentView? = fragment
        fragment?.loadDefaultSettings()
    }

    override fun showToastMessage(message: String, is_long: Boolean) {
        Toast.makeText(
            this,
            message,
            if (is_long) Toast.LENGTH_LONG else Toast.LENGTH_SHORT
        ).show()
    }

    override fun onSettingChanged() {
        presenter.onSettingChanged()
    }

    private val fragment: SettingsFragment?
        get() = supportFragmentManager.findFragmentByTag(FRAGMENT_TAG) as SettingsFragment?

    companion object {
        private const val ARG_MENU_TAG = "menu_tag"
        private const val ARG_GAME_ID = "game_id"
        private const val FRAGMENT_TAG = "settings"

        @JvmStatic
        fun launch(context: Context, menuTag: String?, gameId: String?) {
            val settings = Intent(context, SettingsActivity::class.java)
            settings.putExtra(ARG_MENU_TAG, menuTag)
            settings.putExtra(ARG_GAME_ID, gameId)
            context.startActivity(settings)
        }
    }
}
