// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.ui.main

import android.content.Intent
import android.content.Context
import android.net.Uri
import android.os.Bundle
import android.view.View
import android.view.ViewGroup.MarginLayoutParams
import android.view.WindowManager
import android.view.animation.PathInterpolator
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.activity.viewModels
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.navigation.NavController
import androidx.navigation.fragment.NavHostFragment
import androidx.preference.PreferenceManager
import com.google.android.material.color.MaterialColors
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import java.io.File
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ActivityMainBinding
import org.yuzu.yuzu_emu.dialogs.NetPlayDialog
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.fragments.AddGameFolderDialogFragment
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import android.os.Build
import org.yuzu.yuzu_emu.model.TaskViewModel
import org.yuzu.yuzu_emu.utils.*
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible
import androidx.core.content.edit
import org.yuzu.yuzu_emu.activities.EmulationActivity
import kotlin.text.compareTo
import androidx.core.net.toUri
import com.google.android.material.progressindicator.LinearProgressIndicator
import com.google.android.material.textview.MaterialTextView
import org.yuzu.yuzu_emu.features.settings.model.BooleanSetting
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.updater.APKDownloader
import org.yuzu.yuzu_emu.updater.APKInstaller
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import androidx.documentfile.provider.DocumentFile

class MainActivity : AppCompatActivity(), ThemeProvider {
    private lateinit var binding: ActivityMainBinding

    private val homeViewModel: HomeViewModel by viewModels()
    private val gamesViewModel: GamesViewModel by viewModels()
    private val taskViewModel: TaskViewModel by viewModels()
    private val addonViewModel: AddonViewModel by viewModels()
    private val driverViewModel: DriverViewModel by viewModels()

    override var themeId: Int = 0

    private val CHECKED_DECRYPTION = "CheckedDecryption"
    private var checkedDecryption = false

    override fun attachBaseContext(base: Context) {
        super.attachBaseContext(YuzuApplication.applyLanguage(base))
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()
        splashScreen.setKeepOnScreenCondition { !DirectoryInitialization.areDirectoriesReady }

        ThemeHelper.ThemeChangeListener(this)
        ThemeHelper.setTheme(this)
        super.onCreate(savedInstanceState)
        NativeLibrary.initMultiplayer()

        binding = ActivityMainBinding.inflate(layoutInflater)

        display?.let {
            val supportedModes = it.supportedModes
            val maxRefreshRate = supportedModes.maxByOrNull { mode -> mode.refreshRate }

            if (maxRefreshRate != null) {
                val layoutParams = window.attributes
                layoutParams.preferredDisplayModeId = maxRefreshRate.modeId
                window.attributes = layoutParams
            }
        }

        setContentView(binding.root)

        if (savedInstanceState != null) {
            checkedDecryption = savedInstanceState.getBoolean(CHECKED_DECRYPTION)
        }
        if (!checkedDecryption) {
            val firstTimeSetup = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)
            if (!firstTimeSetup) {
                checkKeys()
            }
            checkedDecryption = true
        }

        WindowCompat.setDecorFitsSystemWindows(window, false)
        window.setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_NOTHING)

        window.statusBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)
        window.navigationBarColor =
            ContextCompat.getColor(applicationContext, android.R.color.transparent)

        binding.statusBarShade.setBackgroundColor(
            ThemeHelper.getColorWithOpacity(
                MaterialColors.getColor(
                    binding.root,
                    com.google.android.material.R.attr.colorSurface
                ),
                ThemeHelper.SYSTEM_BAR_ALPHA
            )
        )
        if (InsetsHelper.getSystemGestureType(applicationContext) != InsetsHelper.GESTURE_NAVIGATION) {
            binding.navigationBarShade.setBackgroundColor(
                ThemeHelper.getColorWithOpacity(
                    MaterialColors.getColor(
                        binding.root,
                        com.google.android.material.R.attr.colorSurface
                    ),
                    ThemeHelper.SYSTEM_BAR_ALPHA
                )
            )
        }

        val navHostFragment =
            supportFragmentManager.findFragmentById(R.id.fragment_container) as NavHostFragment
        setUpNavigation(navHostFragment.navController)

        homeViewModel.statusBarShadeVisible.collect(this) { showStatusBarShade(it) }
        homeViewModel.contentToInstall.collect(
            this,
            resetState = { homeViewModel.setContentToInstall(null) }
        ) {
            if (it != null) {
                installContent(it)
            }
        }
        homeViewModel.checkKeys.collect(this, resetState = { homeViewModel.setCheckKeys(false) }) {
            if (it) checkKeys()
        }

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.stopForegroundService(this)

        val firstTimeSetup = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)

        if (!firstTimeSetup && NativeLibrary.isUpdateCheckerEnabled() && BooleanSetting.ENABLE_UPDATE_CHECKS.getBoolean()) {
             checkForUpdates()
        }
        setInsets()
        applyFullscreenPreference()
    }

    private fun checkForUpdates() {
        Thread {
            val latestVersion = NativeLibrary.checkForUpdate()
            if (latestVersion != null) {
                runOnUiThread {
                    showUpdateDialog(latestVersion)
                }
            }
        }.start()
    }

    // TODO(crueter): body, "View on Forgejo" button
    private fun showUpdateDialog(release: NativeLibrary.UpdateResult) {
        MaterialAlertDialogBuilder(this)
            .setTitle(R.string.update_available)
            .setMessage(getString(R.string.update_available_description, release.title))
            .setPositiveButton(android.R.string.ok) { _, _ ->
                val assets = release.assets

                if (assets.isEmpty()) {
                    openLink(release.url)
                } else {
                    downloadAndInstallUpdate(release)
                }
            }
            .setNeutralButton(R.string.cancel) { dialog, _ ->
                dialog.dismiss()
            }
            .setNegativeButton(R.string.dont_show_again) { dialog, _ ->
                BooleanSetting.ENABLE_UPDATE_CHECKS.setBoolean(false)
                NativeConfig.saveGlobalConfig()
                dialog.dismiss()
            }
            .show()
    }

    private fun openLink(link: String) {
        val intent = Intent(Intent.ACTION_VIEW, link.toUri())
        startActivity(intent)
    }

    private fun downloadAndInstallUpdate(release: NativeLibrary.UpdateResult) {
        CoroutineScope(Dispatchers.IO).launch {
            val packageId = applicationContext.packageName
            val asset = release.assets[0]
            val artifact = asset.split("/").last()
            val apkFile = File(cacheDir, "update-$artifact.apk")

            withContext(Dispatchers.Main) {
                showDownloadProgressDialog()
            }

            val downloader = APKDownloader(asset, apkFile)
            downloader.download(
                onProgress = { progress ->
                    runOnUiThread {
                        updateDownloadProgress(progress)
                    }
                },
                onComplete = { success ->
                    runOnUiThread {
                        dismissDownloadProgressDialog()
                        if (success) {
                            val installer = APKInstaller(this@MainActivity)
                            installer.install(
                                apkFile,
                                onComplete = {
                                    Toast.makeText(
                                        this@MainActivity,
                                        R.string.update_installed_successfully,
                                        Toast.LENGTH_LONG
                                    ).show()
                                },
                                onFailure = { exception ->
                                    Toast.makeText(
                                        this@MainActivity,
                                        getString(R.string.update_install_failed, exception.message),
                                        Toast.LENGTH_LONG
                                    ).show()
                                }
                            )
                        } else {
                            Toast.makeText(
                                this@MainActivity,
                                getString(R.string.update_download_failed) + "\n\nURL: $asset",
                                Toast.LENGTH_LONG
                            ).show()
                        }
                    }
                }
            )
        }
    }

    private var progressDialog: androidx.appcompat.app.AlertDialog? = null
    private var progressBar: LinearProgressIndicator? = null
    private var progressMessage: MaterialTextView? = null

    private fun showDownloadProgressDialog() {
        val progressView = layoutInflater.inflate(R.layout.dialog_download_progress, null)
        progressBar = progressView.findViewById(R.id.download_progress_bar)
        progressMessage = progressView.findViewById(R.id.download_progress_message)

        progressDialog = MaterialAlertDialogBuilder(this)
            .setTitle(R.string.downloading_update)
            .setView(progressView)
            .setCancelable(false)
            .create()
        progressDialog?.show()
    }

    private fun updateDownloadProgress(progress: Int) {
        progressBar?.progress = progress
        progressMessage?.text = getString(R.string.percent, progress)
    }

    private fun dismissDownloadProgressDialog() {
        progressDialog?.dismiss()
        progressDialog = null
        progressBar = null
        progressMessage = null
    }


    fun displayMultiplayerDialog() {
        val dialog = NetPlayDialog(this)
        dialog.show()
    }

    private fun checkKeys() {
        if (!NativeLibrary.areKeysPresent()) {
            MessageDialogFragment.newInstance(
                titleId = R.string.keys_missing,
                descriptionId = R.string.keys_missing_description,
                helpLinkId = R.string.keys_missing_help
            ).show(supportFragmentManager, MessageDialogFragment.TAG)
        }
    }
    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(CHECKED_DECRYPTION, checkedDecryption)
    }

    fun finishSetup(navController: NavController) {
        navController.navigate(R.id.action_firstTimeSetupFragment_to_gamesFragment)
    }

    private fun setUpNavigation(navController: NavController) {
        val firstTimeSetup = PreferenceManager.getDefaultSharedPreferences(applicationContext)
            .getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)

        if (firstTimeSetup && !homeViewModel.navigatedToSetup) {
            navController.navigate(R.id.firstTimeSetupFragment)
            homeViewModel.navigatedToSetup = true
        }
    }

    private fun showStatusBarShade(visible: Boolean) {
        binding.statusBarShade.animate().apply {
            if (visible) {
                binding.statusBarShade.setVisible(true)
                binding.statusBarShade.translationY = binding.statusBarShade.height.toFloat() * -2
                duration = 300
                translationY(0f)
                interpolator = PathInterpolator(0.05f, 0.7f, 0.1f, 1f)
            } else {
                duration = 300
                translationY(binding.statusBarShade.height.toFloat() * -2)
                interpolator = PathInterpolator(0.3f, 0f, 0.8f, 0.15f)
            }
        }.withEndAction {
            if (!visible) {
                binding.statusBarShade.setVisible(visible = false, gone = false)
            }
        }.start()
    }

    override fun onResume() {
        ThemeHelper.setCorrectTheme(this)
        super.onResume()
        applyFullscreenPreference()
    }

    override fun onWindowFocusChanged(hasFocus: Boolean) {
        super.onWindowFocusChanged(hasFocus)
        if (hasFocus) {
            applyFullscreenPreference()
        }
    }

    private fun setInsets() = ViewCompat.setOnApplyWindowInsetsListener(
        binding.root
    ) { _: View, windowInsets: WindowInsetsCompat ->
        val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
        val mlpStatusShade = binding.statusBarShade.layoutParams as MarginLayoutParams
        mlpStatusShade.height = insets.top
        binding.statusBarShade.layoutParams = mlpStatusShade

        // The only situation where we care to have a nav bar shade is when it's at the bottom
        // of the screen where scrolling list elements can go behind it.
        val mlpNavShade = binding.navigationBarShade.layoutParams as MarginLayoutParams
        mlpNavShade.height = insets.bottom
        binding.navigationBarShade.layoutParams = mlpNavShade

        windowInsets
    }

    private fun applyFullscreenPreference() {
        FullscreenHelper.applyToActivity(this)
    }

    override fun setTheme(resId: Int) {
        super.setTheme(resId)
        themeId = resId
    }

    override fun onDestroy() {
        EmulationActivity.stopForegroundService(this)
        super.onDestroy()
    }

    val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                processGamesDir(result)
            }
        }

    val getExternalContentDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                processExternalContentDir(result)
            }
        }

    fun processGamesDir(result: Uri, calledFromGameFragment: Boolean = false) {
        contentResolver.takePersistableUriPermission(
            result,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )

        val uriString = result.toString()
        val folder = gamesViewModel.folders.value.firstOrNull { it.uriString == uriString }
        if (folder != null) {
            Toast.makeText(
                applicationContext,
                R.string.folder_already_added,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        AddGameFolderDialogFragment.newInstance(uriString, calledFromGameFragment)
            .show(supportFragmentManager, AddGameFolderDialogFragment.TAG)
    }

    fun processExternalContentDir(result: Uri) {
        contentResolver.takePersistableUriPermission(
            result,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )

        val uriString = result.toString()
        val folder = gamesViewModel.folders.value.firstOrNull {
            it.uriString == uriString && it.type == org.yuzu.yuzu_emu.model.DirectoryType.EXTERNAL_CONTENT
        }
        if (folder != null) {
            Toast.makeText(
                applicationContext,
                R.string.folder_already_added,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        val externalContentDir = org.yuzu.yuzu_emu.model.GameDir(uriString, false, org.yuzu.yuzu_emu.model.DirectoryType.EXTERNAL_CONTENT)
        gamesViewModel.addFolder(externalContentDir, savedFromGameFragment = false)
    }

    val getProdKey = registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
        if (result != null) {
            processKey(result, "keys")
        }
    }

    val getAmiiboKey = registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
        if (result != null) {
            processKey(result, "bin")
        }
    }

    fun processKey(result: Uri, extension: String = "keys") {
        InstallableActions.processKey(
            activity = this,
            fragmentManager = supportFragmentManager,
            gamesViewModel = gamesViewModel,
            result = result,
            extension = extension
        )
    }

    val getFirmware = registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
        if (result != null) {
            processFirmware(result)
        }
    }

    fun processFirmware(result: Uri, onComplete: (() -> Unit)? = null) {
        InstallableActions.processFirmware(
            activity = this,
            fragmentManager = supportFragmentManager,
            homeViewModel = homeViewModel,
            result = result,
            onComplete = onComplete
        )
    }

    fun uninstallFirmware() {
        InstallableActions.uninstallFirmware(
            activity = this,
            fragmentManager = supportFragmentManager,
            homeViewModel = homeViewModel
        )
    }

    private fun installContent(documents: List<Uri>) {
        InstallableActions.installContent(
            activity = this,
            fragmentManager = supportFragmentManager,
            addonViewModel = addonViewModel,
            documents = documents
        )
    }

    val exportUserData = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/zip")
    ) { result ->
        if (result == null) {
            return@registerForActivityResult
        }
        InstallableActions.exportUserData(
            activity = this,
            fragmentManager = supportFragmentManager,
            result = result
        )
    }

    val importUserData =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null) {
                return@registerForActivityResult
            }
            InstallableActions.importUserData(
                activity = this,
                fragmentManager = supportFragmentManager,
                gamesViewModel = gamesViewModel,
                driverViewModel = driverViewModel,
                result = result
            )
        }
}
