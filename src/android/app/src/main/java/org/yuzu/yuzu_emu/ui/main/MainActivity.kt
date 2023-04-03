// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.main

import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.view.View
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.splashscreen.SplashScreen.Companion.installSplashScreen
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.lifecycle.lifecycleScope
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.ActivityMainBinding
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.ui.platform.PlatformGamesFragment
import org.yuzu.yuzu_emu.utils.*
import java.io.IOException

class MainActivity : AppCompatActivity(), MainView {
    private var platformGamesFragment: PlatformGamesFragment? = null
    private val presenter = MainPresenter(this)

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        val splashScreen = installSplashScreen()
        splashScreen.setKeepOnScreenCondition { !DirectoryInitialization.areDirectoriesReady }

        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)

        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        WindowCompat.setDecorFitsSystemWindows(window, false)

        setSupportActionBar(binding.toolbarMain)
        presenter.onCreate()
        if (savedInstanceState == null) {
            StartupHandler.handleInit(this)
            platformGamesFragment = PlatformGamesFragment()
            supportFragmentManager.beginTransaction()
                .add(R.id.games_platform_frame, platformGamesFragment!!)
                .commit()
        } else {
            platformGamesFragment = supportFragmentManager.getFragment(
                savedInstanceState,
                PlatformGamesFragment.TAG
            ) as PlatformGamesFragment?
        }

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.tryDismissRunningNotification(this)

        setInsets()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        supportFragmentManager.putFragment(
            outState,
            PlatformGamesFragment.TAG,
            platformGamesFragment!!
        )
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_game_grid, menu)
        return true
    }

    /**
     * MainView
     */
    override fun setVersionString(version: String) {
        binding.toolbarMain.subtitle = version
    }

    override fun launchSettingsActivity(menuTag: String) {
        SettingsActivity.launch(this, menuTag, "")
    }

    override fun launchFileListActivity(request: Int) {
        when (request) {
            MainPresenter.REQUEST_ADD_DIRECTORY -> getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data)
            MainPresenter.REQUEST_INSTALL_KEYS -> getProdKey.launch(arrayOf("*/*"))
            MainPresenter.REQUEST_INSTALL_AMIIBO_KEYS -> getAmiiboKey.launch(arrayOf("*/*"))
            MainPresenter.REQUEST_SELECT_GPU_DRIVER -> {
                // Get the driver name for the dialog message.
                var driverName = GpuDriverHelper.customDriverName
                if (driverName == null) {
                    driverName = getString(R.string.system_gpu_driver)
                }

                MaterialAlertDialogBuilder(this)
                    .setTitle(getString(R.string.select_gpu_driver_title))
                    .setMessage(driverName)
                    .setNegativeButton(android.R.string.cancel, null)
                    .setPositiveButton(R.string.select_gpu_driver_default) { _: DialogInterface?, _: Int ->
                        GpuDriverHelper.installDefaultDriver(this)
                        Toast.makeText(
                            this,
                            R.string.select_gpu_driver_use_default,
                            Toast.LENGTH_SHORT
                        ).show()
                    }
                    .setNeutralButton(R.string.select_gpu_driver_install) { _: DialogInterface?, _: Int ->
                        getDriver.launch(arrayOf("application/zip"))
                    }
                    .show()
            }
        }
    }

    /**
     * Called by the framework whenever any actionbar/toolbar icon is clicked.
     *
     * @param item The icon that was clicked on.
     * @return True if the event was handled, false to bubble it up to the OS.
     */
    override fun onOptionsItemSelected(item: MenuItem): Boolean {
        return presenter.handleOptionSelection(item.itemId)
    }

    private fun refreshFragment() {
        if (platformGamesFragment != null) {
            NativeLibrary.resetRomMetadata()
            platformGamesFragment!!.refresh()
        }
    }

    override fun onDestroy() {
        EmulationActivity.tryDismissRunningNotification(this)
        super.onDestroy()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(binding.gamesPlatformFrame) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updatePadding(left = insets.left, right = insets.right)
            InsetsHelper.insetAppBar(insets, binding.appbarMain)
            windowInsets
        }
    }

    private val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            // When a new directory is picked, we currently will reset the existing games
            // database. This effectively means that only one game directory is supported.
            PreferenceManager.getDefaultSharedPreferences(applicationContext).edit()
                .putString(GameHelper.KEY_GAME_PATH, result.toString())
                .apply()
        }

    private val getProdKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val dstPath = DirectoryInitialization.userDirectory + "/keys/"
            if (FileUtil.copyUriToInternalStorage(this, result, dstPath, "prod.keys")) {
                if (NativeLibrary.reloadKeys()) {
                    Toast.makeText(
                        this,
                        R.string.install_keys_success,
                        Toast.LENGTH_SHORT
                    ).show()
                    refreshFragment()
                } else {
                    Toast.makeText(
                        this,
                        R.string.install_keys_failure,
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }

    private val getAmiiboKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val dstPath = DirectoryInitialization.userDirectory + "/keys/"
            if (FileUtil.copyUriToInternalStorage(this, result, dstPath, "key_retail.bin")) {
                if (NativeLibrary.reloadKeys()) {
                    Toast.makeText(
                        this,
                        R.string.install_keys_success,
                        Toast.LENGTH_SHORT
                    ).show()
                    refreshFragment()
                } else {
                    Toast.makeText(
                        this,
                        R.string.install_amiibo_keys_failure,
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }

    private val getDriver =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val progressBinding = DialogProgressBarBinding.inflate(layoutInflater)
            progressBinding.progressBar.isIndeterminate = true
            val installationDialog = MaterialAlertDialogBuilder(this)
                .setTitle(R.string.installing_driver)
                .setView(progressBinding.root)
                .show()

            lifecycleScope.launch {
                withContext(Dispatchers.IO) {
                    // Ignore file exceptions when a user selects an invalid zip
                    try {
                        GpuDriverHelper.installCustomDriver(applicationContext, result)
                    } catch (_: IOException) {
                    }

                    withContext(Dispatchers.Main) {
                        installationDialog.dismiss()

                        val driverName = GpuDriverHelper.customDriverName
                        if (driverName != null) {
                            Toast.makeText(
                                applicationContext,
                                getString(
                                    R.string.select_gpu_driver_install_success,
                                    driverName
                                ),
                                Toast.LENGTH_SHORT
                            ).show()
                        } else {
                            Toast.makeText(
                                applicationContext,
                                R.string.select_gpu_driver_error,
                                Toast.LENGTH_LONG
                            ).show()
                        }
                    }
                }
            }
        }
}
