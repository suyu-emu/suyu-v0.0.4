// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.main

import android.os.Bundle
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.view.animation.PathInterpolator
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.navigation.fragment.NavHostFragment
import androidx.navigation.ui.setupWithNavController
import com.google.android.material.color.MaterialColors
import com.google.android.material.elevation.ElevationOverlayProvider
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.ActivityMainBinding
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.utils.*

class MainActivity : AppCompatActivity() {
    private lateinit var binding: ActivityMainBinding

    private val homeViewModel: HomeViewModel by viewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()
        splashScreen.setKeepOnScreenCondition { !DirectoryInitialization.areDirectoriesReady }

        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        window.statusBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)
        ThemeHelper.setNavigationBarColor(
            this,
            ElevationOverlayProvider(binding.navigationBar.context).compositeOverlay(
                MaterialColors.getColor(binding.navigationBar, R.attr.colorSurface),
                binding.navigationBar.elevation
            )
        )

        // Set up a central host fragment that is controlled via bottom navigation with xml navigation
        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        binding.navigationBar.setupWithNavController(navHostFragment.navController)

        binding.statusBarShade.setBackgroundColor(
            ThemeHelper.getColorWithOpacity(
                MaterialColors.getColor(
                    binding.root,
                    R.attr.colorSurface
                ), ThemeHelper.SYSTEM_BAR_ALPHA
            )
        )

        // Prevents navigation from being drawn for a short time on recreation if set to hidden
        if (homeViewModel.navigationVisible.value == false) {
            binding.navigationBar.visibility = View.INVISIBLE
            binding.statusBarShade.visibility = View.INVISIBLE
        }

        homeViewModel.navigationVisible.observe(this) { visible ->
            showNavigation(visible)
        }
        homeViewModel.statusBarShadeVisible.observe(this) { visible ->
            showStatusBarShade(visible)
        }

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.tryDismissRunningNotification(this)

        setInsets()
    }

    private fun showNavigation(visible: Boolean) {
        binding.navigationBar.animate().apply {
            if (visible) {
                binding.navigationBar.visibility = View.VISIBLE
                binding.navigationBar.translationY = binding.navigationBar.height.toFloat() * 2
                duration = 300
                translationY(0f)
                interpolator = PathInterpolator(0.05f, 0.7f, 0.1f, 1f)
            } else {
                duration = 300
                translationY(binding.navigationBar.height.toFloat() * 2)
                interpolator = PathInterpolator(0.3f, 0f, 0.8f, 0.15f)
            }
        }.withEndAction {
            if (!visible) {
                binding.navigationBar.visibility = View.INVISIBLE
            }
        }.start()
    }

    private fun showStatusBarShade(visible: Boolean) {
        binding.statusBarShade.animate().apply {
            if (visible) {
                binding.statusBarShade.visibility = View.VISIBLE
                binding.statusBarShade.translationY = binding.statusBarShade.height.toFloat() * -2
                duration = 300
                translationY(0f)
                interpolator = PathInterpolator(0.05f, 0.7f, 0.1f, 1f)
            } else {
                duration = 300
                translationY(binding.navigationBar.height.toFloat() * -2)
                interpolator = PathInterpolator(0.3f, 0f, 0.8f, 0.15f)
            }
        }.withEndAction {
            if (!visible) {
                binding.statusBarShade.visibility = View.INVISIBLE
            }
        }.start()
    }

    override fun onDestroy() {
        EmulationActivity.tryDismissRunningNotification(this)
        super.onDestroy()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.statusBarShade) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val mlpShade = view.layoutParams as MarginLayoutParams
            mlpShade.height = insets.top
            binding.statusBarShade.layoutParams = mlpShade
            windowInsets
        }
}
