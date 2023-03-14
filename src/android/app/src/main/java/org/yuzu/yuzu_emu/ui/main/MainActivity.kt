// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.main

import android.content.DialogInterface
import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.Menu
import android.view.MenuItem
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.appcompat.widget.Toolbar
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.model.GameProvider
import org.yuzu.yuzu_emu.ui.platform.PlatformGamesFragment
import org.yuzu.yuzu_emu.utils.*

class MainActivity : AppCompatActivity(), MainView {
    private lateinit var toolbar: Toolbar
    private var platformGamesFragment: PlatformGamesFragment? = null
    private val presenter = MainPresenter(this)

    override fun onCreate(savedInstanceState: Bundle?) {
        ThemeHelper.setTheme(this)

        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)
        findViews()
        setSupportActionBar(toolbar)
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
        PicassoUtils.init()

        // Dismiss previous notifications (should not happen unless a crash occurred)
        EmulationActivity.tryDismissRunningNotification(this)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        supportFragmentManager.putFragment(
            outState,
            PlatformGamesFragment.TAG,
            platformGamesFragment!!
        )
    }

    override fun onResume() {
        super.onResume()
        presenter.addDirIfNeeded(AddDirectoryHelper(this))
    }

    // TODO: Replace with view binding
    private fun findViews() {
        toolbar = findViewById(R.id.toolbar_main)
    }

    override fun onCreateOptionsMenu(menu: Menu): Boolean {
        menuInflater.inflate(R.menu.menu_game_grid, menu)
        return true
    }

    /**
     * MainView
     */
    override fun setVersionString(version: String) {
        toolbar.subtitle = version
    }

    override fun refresh() {
        contentResolver.insert(GameProvider.URI_REFRESH, null)
        refreshFragment()
    }

    override fun launchSettingsActivity(menuTag: String) {
        SettingsActivity.launch(this, menuTag, "")
    }

    override fun launchFileListActivity(request: Int) {
        when (request) {
            MainPresenter.REQUEST_ADD_DIRECTORY -> FileBrowserHelper.openDirectoryPicker(
                this,
                MainPresenter.REQUEST_ADD_DIRECTORY,
                R.string.select_game_folder
            )
            MainPresenter.REQUEST_INSTALL_KEYS -> FileBrowserHelper.openFilePicker(
                this,
                MainPresenter.REQUEST_INSTALL_KEYS,
                R.string.install_keys
            )
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
                        FileBrowserHelper.openFilePicker(
                            this,
                            MainPresenter.REQUEST_SELECT_GPU_DRIVER,
                            R.string.select_gpu_driver
                        )
                    }
                    .show()
            }
        }
    }

    /**
     * @param requestCode An int describing whether the Activity that is returning did so successfully.
     * @param resultCode  An int describing what Activity is giving us this callback.
     * @param result      The information the returning Activity is providing us.
     */
    override fun onActivityResult(requestCode: Int, resultCode: Int, result: Intent?) {
        super.onActivityResult(requestCode, resultCode, result)
        when (requestCode) {
            MainPresenter.REQUEST_ADD_DIRECTORY -> if (resultCode == RESULT_OK) {
                val takeFlags =
                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
                contentResolver.takePersistableUriPermission(
                    Uri.parse(result!!.dataString),
                    takeFlags
                )
                // When a new directory is picked, we currently will reset the existing games
                // database. This effectively means that only one game directory is supported.
                // TODO(bunnei): Consider fixing this in the future, or removing code for this.
                contentResolver.insert(GameProvider.URI_RESET, null)
                // Add the new directory
                presenter.onDirectorySelected(FileBrowserHelper.getSelectedDirectory(result))
            }
            MainPresenter.REQUEST_INSTALL_KEYS -> if (resultCode == RESULT_OK) {
                val takeFlags =
                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
                contentResolver.takePersistableUriPermission(
                    Uri.parse(result!!.dataString),
                    takeFlags
                )
                val dstPath = DirectoryInitialization.userDirectory + "/keys/"
                if (FileUtil.copyUriToInternalStorage(this, result.data, dstPath, "prod.keys")) {
                    if (NativeLibrary.ReloadKeys()) {
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
                        launchFileListActivity(MainPresenter.REQUEST_INSTALL_KEYS)
                    }
                }
            }
            MainPresenter.REQUEST_SELECT_GPU_DRIVER -> if (resultCode == RESULT_OK) {
                val takeFlags =
                    Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
                contentResolver.takePersistableUriPermission(
                    Uri.parse(result!!.dataString),
                    takeFlags
                )
                GpuDriverHelper.installCustomDriver(this, result.data)
                val driverName = GpuDriverHelper.customDriverName
                if (driverName != null) {
                    Toast.makeText(
                        this,
                        getString(R.string.select_gpu_driver_install_success, driverName),
                        Toast.LENGTH_SHORT
                    ).show()
                } else {
                    Toast.makeText(
                        this,
                        R.string.select_gpu_driver_error,
                        Toast.LENGTH_LONG
                    ).show()
                }
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
            NativeLibrary.ResetRomMetadata()
            platformGamesFragment!!.refresh()
        }
    }

    override fun onDestroy() {
        EmulationActivity.tryDismissRunningNotification(this)
        super.onDestroy()
    }
}
