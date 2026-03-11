// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.os.Bundle
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AppCompatActivity
import androidx.core.os.bundleOf
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.navArgs
import com.google.android.material.color.MaterialColors
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.ActivitySettingsBinding
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.InsetsHelper
import org.yuzu.yuzu_emu.utils.ThemeHelper

enum class SettingsSubscreen {
    PROFILE_MANAGER,
    DRIVER_MANAGER,
    DRIVER_FETCHER,
    FREEDRENO_SETTINGS,
    APPLET_LAUNCHER,
    INSTALLABLE,
    GAME_FOLDERS,
    ABOUT,
    LICENSES,
    GAME_INFO,
    ADDONS,
}

class SettingsSubscreenActivity : AppCompatActivity() {
    private lateinit var binding: ActivitySettingsBinding

    private val args by navArgs<SettingsSubscreenActivityArgs>()

    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(YuzuApplication.applyLanguage(base))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivitySettingsBinding.inflate(layoutInflater)
        setContentView(binding.root)

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        if (savedInstanceState == null) {
            val navController = navHostFragment.navController
            val navGraph = navController.navInflater.inflate(
                R.navigation.settings_subscreen_navigation
            )
            navGraph.setStartDestination(resolveStartDestination())
            navController.setGraph(navGraph, createStartDestinationArgs())
        }

        WindowCompat.setDecorFitsSystemWindows(window, false)

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

    override fun onStart() {
        super.onStart()
        if (!DirectoryInitialization.areDirectoriesReady) {
            DirectoryInitialization.start()
        }
    }

    fun navigateBack() {
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        if (!navHostFragment.navController.popBackStack()) {
            finish()
        }
    }

    private fun resolveStartDestination(): Int =
        when (args.destination) {
            SettingsSubscreen.PROFILE_MANAGER -> R.id.profileManagerFragment
            SettingsSubscreen.DRIVER_MANAGER -> R.id.driverManagerFragment
            SettingsSubscreen.DRIVER_FETCHER -> R.id.driverFetcherFragment
            SettingsSubscreen.FREEDRENO_SETTINGS -> R.id.freedrenoSettingsFragment
            SettingsSubscreen.APPLET_LAUNCHER -> R.id.appletLauncherFragment
            SettingsSubscreen.INSTALLABLE -> R.id.installableFragment
            SettingsSubscreen.GAME_FOLDERS -> R.id.gameFoldersFragment
            SettingsSubscreen.ABOUT -> R.id.aboutFragment
            SettingsSubscreen.LICENSES -> R.id.licensesFragment
            SettingsSubscreen.GAME_INFO -> R.id.gameInfoFragment
            SettingsSubscreen.ADDONS -> R.id.addonsFragment
        }

    private fun createStartDestinationArgs(): Bundle =
        when (args.destination) {
            SettingsSubscreen.DRIVER_MANAGER,
            SettingsSubscreen.FREEDRENO_SETTINGS -> bundleOf("game" to args.game)

            SettingsSubscreen.GAME_INFO,
            SettingsSubscreen.ADDONS -> bundleOf(
                "game" to requireNotNull(args.game) {
                    "Game is required for ${args.destination}"
                }
            )

            else -> Bundle()
        }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.navigationBarShade
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())

            val mlpNavShade = binding.navigationBarShade.layoutParams as MarginLayoutParams
            mlpNavShade.height = barInsets.bottom
            binding.navigationBarShade.layoutParams = mlpNavShade

            windowInsets
        }
    }
}
