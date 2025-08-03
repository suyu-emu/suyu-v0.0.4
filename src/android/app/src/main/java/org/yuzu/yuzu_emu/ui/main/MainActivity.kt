// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.ui.main

import android.content.Intent
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
import java.io.File
import java.io.FilenameFilter
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ActivityMainBinding
import org.yuzu.yuzu_emu.dialogs.NetPlayDialog
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.fragments.AddGameFolderDialogFragment
import org.yuzu.yuzu_emu.fragments.ProgressDialogFragment
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.InstallResult
import org.yuzu.yuzu_emu.model.TaskState
import org.yuzu.yuzu_emu.model.TaskViewModel
import org.yuzu.yuzu_emu.utils.*
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream
import androidx.core.content.edit

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

    private val CHECKED_FIRMWARE = "CheckedFirmware"
    private var checkedFirmware = false

    private val requestBluetoothPermissionsLauncher =
        registerForActivityResult(
            androidx.activity.result.contract.ActivityResultContracts.RequestMultiplePermissions()
        ) { permissions ->
            val granted = permissions.entries.all { it.value }
            if (granted) {
                // Permissions were granted.
                Toast.makeText(this, R.string.bluetooth_permissions_granted, Toast.LENGTH_SHORT)
                    .show()
            } else {
                // Permissions were denied.
                Toast.makeText(this, R.string.bluetooth_permissions_denied, Toast.LENGTH_LONG)
                    .show()
            }
        }

    private fun checkAndRequestBluetoothPermissions() {
        // This check is only necessary for Android 12 (API level 31) and above.
        if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.S) {
            val permissionsToRequest = arrayOf(
                android.Manifest.permission.BLUETOOTH_SCAN,
                android.Manifest.permission.BLUETOOTH_CONNECT
            )

            val permissionsNotGranted = permissionsToRequest.filter {
                checkSelfPermission(it) != android.content.pm.PackageManager.PERMISSION_GRANTED
            }

            if (permissionsNotGranted.isNotEmpty()) {
                requestBluetoothPermissionsLauncher.launch(permissionsNotGranted.toTypedArray())
            }
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()
        splashScreen.setKeepOnScreenCondition { !DirectoryInitialization.areDirectoriesReady }

        ThemeHelper.ThemeChangeListener(this)
        ThemeHelper.setTheme(this)
        super.onCreate(savedInstanceState)
        NativeLibrary.initMultiplayer()

        binding = ActivityMainBinding.inflate(layoutInflater)

        setContentView(binding.root)

        checkAndRequestBluetoothPermissions()

        if (savedInstanceState != null) {
            checkedDecryption = savedInstanceState.getBoolean(CHECKED_DECRYPTION)
            checkedFirmware = savedInstanceState.getBoolean(CHECKED_FIRMWARE)
        }
        if (!checkedDecryption) {
            val firstTimeSetup = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)
            if (!firstTimeSetup) {
                checkKeys()
            }
            checkedDecryption = true
        }

        if (!checkedFirmware) {
            val firstTimeSetup = PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getBoolean(Settings.PREF_FIRST_APP_LAUNCH, true)
            if (!firstTimeSetup) {
                checkFirmware()
                showPreAlphaWarningDialog()
            }
            checkedFirmware = true
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

        homeViewModel.checkFirmware.collect(
            this,
            resetState = { homeViewModel.setCheckFirmware(false) }
        ) {
            if (it) checkFirmware()
        }

        setInsets()
    }

    fun showPreAlphaWarningDialog() {
        val shouldDisplayAlphaWarning =
            PreferenceManager.getDefaultSharedPreferences(applicationContext)
                .getBoolean(Settings.PREF_SHOULD_SHOW_PRE_ALPHA_WARNING, true)
        if (shouldDisplayAlphaWarning) {
            MessageDialogFragment.newInstance(
                this,
                titleId = R.string.pre_alpha_warning_title,
                descriptionId = R.string.pre_alpha_warning_description,
                positiveButtonTitleId = R.string.dont_show_again,
                negativeButtonTitleId = R.string.close,
                showNegativeButton = true,
                positiveAction = {
                    PreferenceManager.getDefaultSharedPreferences(applicationContext).edit() {
                        putBoolean(Settings.PREF_SHOULD_SHOW_PRE_ALPHA_WARNING, false)
                    }
                }
            ).show(supportFragmentManager, MessageDialogFragment.TAG)
        }
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

    private fun checkFirmware() {
        val resultCode: Int = NativeLibrary.verifyFirmware()
        if (resultCode == 0) return

        val resultString: String =
            resources.getStringArray(R.array.verifyFirmwareResults)[resultCode]

        MessageDialogFragment.newInstance(
            titleId = R.string.firmware_invalid,
            descriptionString = resultString,
            helpLinkId = R.string.firmware_missing_help
        ).show(supportFragmentManager, MessageDialogFragment.TAG)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(CHECKED_DECRYPTION, checkedDecryption)
        outState.putBoolean(CHECKED_FIRMWARE, checkedFirmware)
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

    override fun setTheme(resId: Int) {
        super.setTheme(resId)
        themeId = resId
    }

    val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                processGamesDir(result)
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
        contentResolver.takePersistableUriPermission(
            result,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )

        val resultCode: Int = NativeLibrary.installKeys(result.toString(), extension)

        if (resultCode == 0) {
            // TODO(crueter): It may be worth it to switch some of these Toasts to snackbars,
            // since most of it is foreground-only anyways.
            Toast.makeText(
                applicationContext,
                R.string.keys_install_success,
                Toast.LENGTH_SHORT
            ).show()

            gamesViewModel.reloadGames(true)

            return
        }

        val resultString: String =
            resources.getStringArray(R.array.installKeysResults)[resultCode]

        MessageDialogFragment.newInstance(
            titleId = R.string.keys_failed,
            descriptionString = resultString,
            helpLinkId = R.string.keys_missing_help
        ).show(supportFragmentManager, MessageDialogFragment.TAG)
    }

    val getFirmware = registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
        if (result != null) {
            processFirmware(result)
        }
    }

    fun processFirmware(result: Uri, onComplete: (() -> Unit)? = null) {
        val filterNCA = FilenameFilter { _, dirName -> dirName.endsWith(".nca") }

        val firmwarePath =
            File(DirectoryInitialization.userDirectory + "/nand/system/Contents/registered/")
        val cacheFirmwareDir = File("${cacheDir.path}/registered/")

        ProgressDialogFragment.newInstance(
            this,
            R.string.firmware_installing
        ) { progressCallback, _ ->
            var messageToShow: Any
            try {
                FileUtil.unzipToInternalStorage(
                    result.toString(),
                    cacheFirmwareDir,
                    progressCallback
                )
                val unfilteredNumOfFiles = cacheFirmwareDir.list()?.size ?: -1
                val filteredNumOfFiles = cacheFirmwareDir.list(filterNCA)?.size ?: -2
                messageToShow = if (unfilteredNumOfFiles != filteredNumOfFiles) {
                    MessageDialogFragment.newInstance(
                        this,
                        titleId = R.string.firmware_installed_failure,
                        descriptionId = R.string.firmware_installed_failure_description
                    )
                } else {
                    firmwarePath.deleteRecursively()
                    cacheFirmwareDir.copyRecursively(firmwarePath, true)
                    NativeLibrary.initializeSystem(true)
                    homeViewModel.setCheckKeys(true)
                    homeViewModel.setCheckFirmware(true)
                    getString(R.string.save_file_imported_success)
                }
            } catch (e: Exception) {
                Log.error("[MainActivity] Firmware install failed - ${e.message}")
                messageToShow = getString(R.string.fatal_error)
            } finally {
                cacheFirmwareDir.deleteRecursively()
            }
            messageToShow
        }.apply {
            onDialogComplete = onComplete
        }.show(supportFragmentManager, ProgressDialogFragment.TAG)
    }

    fun uninstallFirmware() {
        val firmwarePath =
            File(DirectoryInitialization.userDirectory + "/nand/system/Contents/registered/")
        ProgressDialogFragment.newInstance(
            this,
            R.string.firmware_uninstalling
        ) { progressCallback, _ ->
            var messageToShow: Any
            try {
                // Ensure the firmware directory exists before attempting to delete
                if (firmwarePath.exists()) {
                    firmwarePath.deleteRecursively()
                    // Optionally reinitialize the system or perform other necessary steps
                    NativeLibrary.initializeSystem(true)
                    homeViewModel.setCheckKeys(true)
                    homeViewModel.setCheckFirmware(true)
                    messageToShow = getString(R.string.firmware_uninstalled_success)
                } else {
                    messageToShow = getString(R.string.firmware_uninstalled_failure)
                }
            } catch (e: Exception) {
                Log.error("[MainActivity] Firmware uninstall failed - ${e.message}")
                messageToShow = getString(R.string.fatal_error)
            }
            messageToShow
        }.show(supportFragmentManager, ProgressDialogFragment.TAG)
    }

    val installGameUpdate = registerForActivityResult(
        ActivityResultContracts.OpenMultipleDocuments()
    ) { documents: List<Uri> ->
        if (documents.isEmpty()) {
            return@registerForActivityResult
        }

        if (addonViewModel.game == null) {
            installContent(documents)
            return@registerForActivityResult
        }

        ProgressDialogFragment.newInstance(
            this@MainActivity,
            R.string.verifying_content,
            false
        ) { _, _ ->
            var updatesMatchProgram = true
            for (document in documents) {
                val valid = NativeLibrary.doesUpdateMatchProgram(
                    addonViewModel.game!!.programId,
                    document.toString()
                )
                if (!valid) {
                    updatesMatchProgram = false
                    break
                }
            }

            if (updatesMatchProgram) {
                homeViewModel.setContentToInstall(documents)
            } else {
                MessageDialogFragment.newInstance(
                    this@MainActivity,
                    titleId = R.string.content_install_notice,
                    descriptionId = R.string.content_install_notice_description,
                    positiveAction = { homeViewModel.setContentToInstall(documents) },
                    negativeAction = {}
                )
            }
        }.show(supportFragmentManager, ProgressDialogFragment.TAG)
    }

    private fun installContent(documents: List<Uri>) {
        ProgressDialogFragment.newInstance(
            this@MainActivity,
            R.string.installing_game_content
        ) { progressCallback, messageCallback ->
            var installSuccess = 0
            var installOverwrite = 0
            var errorBaseGame = 0
            var error = 0
            documents.forEach {
                messageCallback.invoke(FileUtil.getFilename(it))
                when (
                    InstallResult.from(
                        NativeLibrary.installFileToNand(
                            it.toString(),
                            progressCallback
                        )
                    )
                ) {
                    InstallResult.Success -> {
                        installSuccess += 1
                    }

                    InstallResult.Overwrite -> {
                        installOverwrite += 1
                    }

                    InstallResult.BaseInstallAttempted -> {
                        errorBaseGame += 1
                    }

                    InstallResult.Failure -> {
                        error += 1
                    }
                }
            }

            addonViewModel.refreshAddons()

            val separator = System.lineSeparator() ?: "\n"
            val installResult = StringBuilder()
            if (installSuccess > 0) {
                installResult.append(
                    getString(
                        R.string.install_game_content_success_install,
                        installSuccess
                    )
                )
                installResult.append(separator)
            }
            if (installOverwrite > 0) {
                installResult.append(
                    getString(
                        R.string.install_game_content_success_overwrite,
                        installOverwrite
                    )
                )
                installResult.append(separator)
            }
            val errorTotal: Int = errorBaseGame + error
            if (errorTotal > 0) {
                installResult.append(separator)
                installResult.append(
                    getString(
                        R.string.install_game_content_failed_count,
                        errorTotal
                    )
                )
                installResult.append(separator)
                if (errorBaseGame > 0) {
                    installResult.append(separator)
                    installResult.append(
                        getString(R.string.install_game_content_failure_base)
                    )
                    installResult.append(separator)
                }
                if (error > 0) {
                    installResult.append(
                        getString(R.string.install_game_content_failure_description)
                    )
                    installResult.append(separator)
                }
                return@newInstance MessageDialogFragment.newInstance(
                    this,
                    titleId = R.string.install_game_content_failure,
                    descriptionString = installResult.toString().trim(),
                    helpLinkId = R.string.install_game_content_help_link
                )
            } else {
                return@newInstance MessageDialogFragment.newInstance(
                    this,
                    titleId = R.string.install_game_content_success,
                    descriptionString = installResult.toString().trim()
                )
            }
        }.show(supportFragmentManager, ProgressDialogFragment.TAG)
    }

    val exportUserData = registerForActivityResult(
        ActivityResultContracts.CreateDocument("application/zip")
    ) { result ->
        if (result == null) {
            return@registerForActivityResult
        }

        ProgressDialogFragment.newInstance(
            this,
            R.string.exporting_user_data,
            true
        ) { progressCallback, _ ->
            val zipResult = FileUtil.zipFromInternalStorage(
                File(DirectoryInitialization.userDirectory!!),
                DirectoryInitialization.userDirectory!!,
                BufferedOutputStream(contentResolver.openOutputStream(result)),
                progressCallback,
                compression = false
            )
            return@newInstance when (zipResult) {
                TaskState.Completed -> getString(R.string.user_data_export_success)
                TaskState.Failed -> R.string.export_failed
                TaskState.Cancelled -> R.string.user_data_export_cancelled
            }
        }.show(supportFragmentManager, ProgressDialogFragment.TAG)
    }

    val importUserData =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null) {
                return@registerForActivityResult
            }

            ProgressDialogFragment.newInstance(
                this,
                R.string.importing_user_data
            ) { progressCallback, _ ->
                val checkStream =
                    ZipInputStream(BufferedInputStream(contentResolver.openInputStream(result)))
                var isYuzuBackup = false
                checkStream.use { stream ->
                    var ze: ZipEntry? = null
                    while (stream.nextEntry?.also { ze = it } != null) {
                        val itemName = ze!!.name.trim()
                        if (itemName == "/config/config.ini" || itemName == "config/config.ini") {
                            isYuzuBackup = true
                            return@use
                        }
                    }
                }
                if (!isYuzuBackup) {
                    return@newInstance MessageDialogFragment.newInstance(
                        this,
                        titleId = R.string.invalid_yuzu_backup,
                        descriptionId = R.string.user_data_import_failed_description
                    )
                }

                // Clear existing user data
                NativeConfig.unloadGlobalConfig()
                File(DirectoryInitialization.userDirectory!!).deleteRecursively()

                // Copy archive to internal storage
                try {
                    FileUtil.unzipToInternalStorage(
                        result.toString(),
                        File(DirectoryInitialization.userDirectory!!),
                        progressCallback
                    )
                } catch (e: Exception) {
                    return@newInstance MessageDialogFragment.newInstance(
                        this,
                        titleId = R.string.import_failed,
                        descriptionId = R.string.user_data_import_failed_description
                    )
                }

                // Reinitialize relevant data
                NativeLibrary.initializeSystem(true)
                NativeConfig.initializeGlobalConfig()
                gamesViewModel.reloadGames(false)
                driverViewModel.reloadDriverData()

                return@newInstance getString(R.string.user_data_import_success)
            }.show(supportFragmentManager, ProgressDialogFragment.TAG)
        }
}
