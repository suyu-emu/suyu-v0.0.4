// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.os.Bundle
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.navArgs
import com.google.android.material.color.MaterialColors
import org.yuzu.yuzu_emu.NativeLibrary
import java.io.IOException
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ActivitySettingsBinding
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.fragments.ResetSettingsDialogFragment
import org.yuzu.yuzu_emu.model.SettingsViewModel
import org.yuzu.yuzu_emu.utils.*

class SettingsActivity : AppCompatActivity() {
    private lateinit var binding: ActivitySettingsBinding

    private val args by navArgs<SettingsActivityArgs>()

    private val settingsViewModel: SettingsViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        settingsViewModel.game = args.game

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        navHostFragment.navController.setGraph(R.navigation.settings_navigation, intent.extras)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        if (savedInstanceState != null) {
            settingsViewModel.shouldSave = savedInstanceState.getBoolean(KEY_SHOULD_SAVE)
        }

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

        settingsViewModel.shouldRecreate.observe(this) {
            if (it) {
                settingsViewModel.setShouldRecreate(false)
                recreate()
            }
        }
        settingsViewModel.shouldNavigateBack.observe(this) {
            if (it) {
                settingsViewModel.setShouldNavigateBack(false)
                navigateBack()
            }
        }
        settingsViewModel.shouldShowResetSettingsDialog.observe(this) {
            if (it) {
                settingsViewModel.setShouldShowResetSettingsDialog(false)
                ResetSettingsDialogFragment().show(
                    supportFragmentManager,
                    ResetSettingsDialogFragment.TAG
                )
            }
        }

        onBackPressedDispatcher.addCallback(
            this,
            object : OnBackPressedCallback(true) {
                override fun handleOnBackPressed() = navigateBack()
            }
        )

        setInsets()
    }

    fun navigateBack() {
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        if (navHostFragment.childFragmentManager.backStackEntryCount > 0) {
            navHostFragment.navController.popBackStack()
        } else {
            finish()
        }
    }

    override fun onSaveInstanceState(outState: Bundle) {
        // Critical: If super method is not called, rotations will be busted.
        super.onSaveInstanceState(outState)
        outState.putBoolean(KEY_SHOULD_SAVE, settingsViewModel.shouldSave)
    }

    override fun onStart() {
        super.onStart()
        // TODO: Load custom settings contextually
        if (!DirectoryInitialization.areDirectoriesReady) {
            DirectoryInitialization.start()
        }
    }

    /**
     * If this is called, the user has left the settings screen (potentially through the
     * home button) and will expect their changes to be persisted. So we kick off an
     * IntentService which will do so on a background thread.
     */
    override fun onStop() {
        super.onStop()
        if (isFinishing && settingsViewModel.shouldSave) {
            Log.debug("[SettingsActivity] Settings activity stopping. Saving settings to INI...")
            Settings.saveSettings()
            NativeLibrary.reloadSettings()
        }
    }

    override fun onDestroy() {
        settingsViewModel.clear()
        super.onDestroy()
    }

    fun onSettingsReset() {
        // Prevents saving to a non-existent settings file
        settingsViewModel.shouldSave = false

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

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.navigationBarShade
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            val mlpShade = view.layoutParams as MarginLayoutParams
            mlpShade.height = barInsets.bottom
            view.layoutParams = mlpShade

            windowInsets
        }
    }

    companion object {
        private const val KEY_SHOULD_SAVE = "should_save"
    }
}
