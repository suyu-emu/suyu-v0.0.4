// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import com.google.android.material.color.MaterialColors
import java.io.IOException
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ActivitySettingsBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.utils.*

class SettingsActivity : AppCompatActivity(), SettingsActivityView {
    private val presenter = SettingsActivityPresenter(this)

    private lateinit var binding: ActivitySettingsBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        val launcher = intent
        val gameID = launcher.getStringExtra(ARG_GAME_ID)
        val menuTag = launcher.getStringExtra(ARG_MENU_TAG)
        presenter.onCreate(savedInstanceState, menuTag!!, gameID!!)

        // Show "Back" button in the action bar for navigation
        setSupportActionBar(binding.toolbarSettings)
        supportActionBar!!.setDisplayHomeAsUpEnabled(true)

        if (InsetsHelper.getSystemGestureType(applicationContext) !=
            InsetsHelper.GESTURE_NAVIGATION
        ) {
            binding.navigationBarShade.setBackgroundColor(
                ThemeHelper.getColorWithOpacity(
                    MaterialColors.getColor(
                        binding.navigationBarShade,
                        com.google.android.material.R.attr.colorSurface
                    ),
                    ThemeHelper.SYSTEM_BAR_ALPHA
                )
            )
        }

        onBackPressedDispatcher.addCallback(
            this,
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() = navigateBack()
            }
        )

        setInsets()
    }

    override fun onSupportNavigateUp(): Boolean {
        navigateBack()
        return true
    }

    private fun navigateBack() {
        if (supportFragmentManager.backStackEntryCount > 0) {
            supportFragmentManager.popBackStack()
        } else {
            finish()
        }
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
    }

    override fun showSettingsFragment(menuTag: String, addToStack: Boolean, gameId: String) {
        if (!addToStack && settingsFragment != null) {
            return
        }

        val transaction = supportFragmentManager.beginTransaction()
        if (addToStack) {
            if (areSystemAnimationsEnabled()) {
                transaction.setCustomAnimations(
                    R.anim.anim_settings_fragment_in,
                    R.anim.anim_settings_fragment_out,
                    0,
                    R.anim.anim_pop_settings_fragment_out
                )
            }
            transaction.addToBackStack(null)
        }
        transaction.replace(
            R.id.frame_content,
            SettingsFragment.newInstance(menuTag, gameId),
            FRAGMENT_TAG
        )
        transaction.commit()
    }

    private fun areSystemAnimationsEnabled(): Boolean {
        val duration = android.provider.Settings.Global.getFloat(
            contentResolver,
            android.provider.Settings.Global.ANIMATOR_DURATION_SCALE,
            1f
        )
        val transition = android.provider.Settings.Global.getFloat(
            contentResolver,
            android.provider.Settings.Global.TRANSITION_ANIMATION_SCALE,
            1f
        )
        return duration != 0f && transition != 0f
    }

    override fun onSettingsFileLoaded() {
        val fragment: SettingsFragmentView? = settingsFragment
        fragment?.loadSettingsList()
    }

    override fun onSettingsFileNotFound() {
        val fragment: SettingsFragmentView? = settingsFragment
        fragment?.loadSettingsList()
    }

    override fun onSettingChanged() {
        presenter.onSettingChanged()
    }

    fun onSettingsReset() {
        // Prevents saving to a non-existent settings file
        presenter.onSettingsReset()

        // Delete settings file because the user may have changed values that do not exist in the UI
        val settingsFile = SettingsFile.getSettingsFile(SettingsFile.FILE_NAME_CONFIG)
        if (!settingsFile.delete()) {
            throw IOException("Failed to delete $settingsFile")
        }
        Settings.settingsList.forEach { it.reset() }

        Toast.makeText(
            applicationContext,
            getString(R.string.settings_reset),
            Toast.LENGTH_LONG
        ).show()
        finish()
    }

    fun setToolbarTitle(title: String) {
        binding.toolbarSettingsLayout.title = title
    }

    private val settingsFragment: SettingsFragment?
        get() = supportFragmentManager.findFragmentByTag(FRAGMENT_TAG) as SettingsFragment?

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.frameContent
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())
            view.updatePadding(
                left = barInsets.left + cutoutInsets.left,
                right = barInsets.right + cutoutInsets.right
            )

            val mlpAppBar = binding.appbarSettings.layoutParams as MarginLayoutParams
            mlpAppBar.leftMargin = barInsets.left + cutoutInsets.left
            mlpAppBar.rightMargin = barInsets.right + cutoutInsets.right
            binding.appbarSettings.layoutParams = mlpAppBar

            val mlpShade = binding.navigationBarShade.layoutParams as MarginLayoutParams
            mlpShade.height = barInsets.bottom
            binding.navigationBarShade.layoutParams = mlpShade

            windowInsets
        }
    }

    companion object {
        private const val ARG_MENU_TAG = "menu_tag"
        private const val ARG_GAME_ID = "game_id"
        private const val FRAGMENT_TAG = "settings"

        fun launch(context: Context, menuTag: String?, gameId: String?) {
            val settings = Intent(context, SettingsActivity::class.java)
            settings.putExtra(ARG_MENU_TAG, menuTag)
            settings.putExtra(ARG_GAME_ID, gameId)
            context.startActivity(settings)
        }
    }
}
