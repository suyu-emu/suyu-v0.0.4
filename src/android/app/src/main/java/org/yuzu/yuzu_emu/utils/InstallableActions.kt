// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Intent
import android.net.Uri
import android.widget.Toast
import androidx.fragment.app.FragmentActivity
import androidx.fragment.app.FragmentManager
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.fragments.ProgressDialogFragment
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.model.InstallResult
import org.yuzu.yuzu_emu.model.TaskState
import java.io.BufferedInputStream
import java.io.BufferedOutputStream
import java.io.File
import java.io.FilenameFilter
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream

object InstallableActions {
    private fun verifyGameContentAndInstall(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        documents: List<Uri>,
        programId: String?,
        onInstallConfirmed: () -> Unit
    ) {
        if (documents.isEmpty()) {
            return
        }

        if (programId == null) {
            onInstallConfirmed()
            return
        }

        ProgressDialogFragment.newInstance(
            activity,
            R.string.verifying_content,
            false
        ) { _, _ ->
            var updatesMatchProgram = true
            for (document in documents) {
                val valid = NativeLibrary.doesUpdateMatchProgram(
                    programId,
                    document.toString()
                )
                if (!valid) {
                    updatesMatchProgram = false
                    break
                }
            }

            activity.runOnUiThread {
                if (updatesMatchProgram) {
                    onInstallConfirmed()
                } else {
                    MessageDialogFragment.newInstance(
                        activity,
                        titleId = R.string.content_install_notice,
                        descriptionId = R.string.content_install_notice_description,
                        positiveAction = onInstallConfirmed,
                        negativeAction = {}
                    ).show(fragmentManager, MessageDialogFragment.TAG)
                }
            }
            return@newInstance Any()
        }.show(fragmentManager, ProgressDialogFragment.TAG)
    }

    fun verifyAndInstallContent(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        addonViewModel: AddonViewModel,
        documents: List<Uri>,
        programId: String?
    ) {
        verifyGameContentAndInstall(
            activity = activity,
            fragmentManager = fragmentManager,
            documents = documents,
            programId = programId
        ) {
            installContent(
                activity = activity,
                fragmentManager = fragmentManager,
                addonViewModel = addonViewModel,
                documents = documents
            )
        }
    }

    fun processKey(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        gamesViewModel: GamesViewModel,
        result: Uri,
        extension: String = "keys"
    ) {
        activity.contentResolver.takePersistableUriPermission(
            result,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )

        val resultCode = NativeLibrary.installKeys(result.toString(), extension)
        if (resultCode == 0) {
            Toast.makeText(
                activity.applicationContext,
                R.string.keys_install_success,
                Toast.LENGTH_SHORT
            ).show()
            gamesViewModel.reloadGames(true)
            return
        }

        val resultString = activity.resources.getStringArray(R.array.installKeysResults)[resultCode]
        MessageDialogFragment.newInstance(
            titleId = R.string.keys_failed,
            descriptionString = resultString,
            helpLinkId = R.string.keys_missing_help
        ).show(fragmentManager, MessageDialogFragment.TAG)
    }

    fun processFirmware(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        homeViewModel: HomeViewModel,
        result: Uri,
        onComplete: (() -> Unit)? = null
    ) {
        val filterNCA = FilenameFilter { _, dirName -> dirName.endsWith(".nca") }
        val firmwarePath = File(NativeConfig.getNandDir() + "/system/Contents/registered/")
        val cacheFirmwareDir = File("${activity.cacheDir.path}/registered/")

        ProgressDialogFragment.newInstance(
            activity,
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
                        activity,
                        titleId = R.string.firmware_installed_failure,
                        descriptionId = R.string.firmware_installed_failure_description
                    )
                } else {
                    firmwarePath.deleteRecursively()
                    cacheFirmwareDir.copyRecursively(firmwarePath, overwrite = true)
                    NativeLibrary.initializeSystem(true)
                    homeViewModel.setCheckKeys(true)
                    activity.getString(R.string.save_file_imported_success)
                }
            } catch (_: Exception) {
                messageToShow = activity.getString(R.string.fatal_error)
            } finally {
                cacheFirmwareDir.deleteRecursively()
            }
            messageToShow
        }.apply {
            onDialogComplete = onComplete
        }.show(fragmentManager, ProgressDialogFragment.TAG)
    }

    fun uninstallFirmware(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        homeViewModel: HomeViewModel
    ) {
        val firmwarePath = File(NativeConfig.getNandDir() + "/system/Contents/registered/")
        ProgressDialogFragment.newInstance(
            activity,
            R.string.firmware_uninstalling
        ) { _, _ ->
            val messageToShow: Any = try {
                if (firmwarePath.exists()) {
                    firmwarePath.deleteRecursively()
                    NativeLibrary.initializeSystem(true)
                    homeViewModel.setCheckKeys(true)
                    activity.getString(R.string.firmware_uninstalled_success)
                } else {
                    activity.getString(R.string.firmware_uninstalled_failure)
                }
            } catch (_: Exception) {
                activity.getString(R.string.fatal_error)
            }
            messageToShow
        }.show(fragmentManager, ProgressDialogFragment.TAG)
    }

    fun installContent(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        addonViewModel: AddonViewModel,
        documents: List<Uri>
    ) {
        ProgressDialogFragment.newInstance(
            activity,
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
                    InstallResult.Success -> installSuccess += 1
                    InstallResult.Overwrite -> installOverwrite += 1
                    InstallResult.BaseInstallAttempted -> errorBaseGame += 1
                    InstallResult.Failure -> error += 1
                }
            }

            addonViewModel.refreshAddons(force = true)

            val separator = System.lineSeparator() ?: "\n"
            val installResult = StringBuilder()
            if (installSuccess > 0) {
                installResult.append(
                    activity.getString(
                        R.string.install_game_content_success_install,
                        installSuccess
                    )
                )
                installResult.append(separator)
            }
            if (installOverwrite > 0) {
                installResult.append(
                    activity.getString(
                        R.string.install_game_content_success_overwrite,
                        installOverwrite
                    )
                )
                installResult.append(separator)
            }
            val errorTotal = errorBaseGame + error
            if (errorTotal > 0) {
                installResult.append(separator)
                installResult.append(
                    activity.getString(
                        R.string.install_game_content_failed_count,
                        errorTotal
                    )
                )
                installResult.append(separator)
                if (errorBaseGame > 0) {
                    installResult.append(separator)
                    installResult.append(activity.getString(R.string.install_game_content_failure_base))
                    installResult.append(separator)
                }
                if (error > 0) {
                    installResult.append(
                        activity.getString(R.string.install_game_content_failure_description)
                    )
                    installResult.append(separator)
                }
                return@newInstance MessageDialogFragment.newInstance(
                    activity,
                    titleId = R.string.install_game_content_failure,
                    descriptionString = installResult.toString().trim(),
                    helpLinkId = R.string.install_game_content_help_link
                )
            } else {
                return@newInstance MessageDialogFragment.newInstance(
                    activity,
                    titleId = R.string.install_game_content_success,
                    descriptionString = installResult.toString().trim()
                )
            }
        }.show(fragmentManager, ProgressDialogFragment.TAG)
    }

    fun exportUserData(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        result: Uri
    ) {
        val userDirectory = DirectoryInitialization.userDirectory
        if (userDirectory == null) {
            Toast.makeText(
                activity.applicationContext,
                R.string.fatal_error,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        ProgressDialogFragment.newInstance(
            activity,
            R.string.exporting_user_data,
            true
        ) { progressCallback, _ ->
            val zipResult = FileUtil.zipFromInternalStorage(
                File(userDirectory),
                userDirectory,
                BufferedOutputStream(activity.contentResolver.openOutputStream(result)),
                progressCallback,
                compression = false
            )
            return@newInstance when (zipResult) {
                TaskState.Completed -> activity.getString(R.string.user_data_export_success)
                TaskState.Failed -> R.string.export_failed
                TaskState.Cancelled -> R.string.user_data_export_cancelled
            }
        }.show(fragmentManager, ProgressDialogFragment.TAG)
    }

    fun importUserData(
        activity: FragmentActivity,
        fragmentManager: FragmentManager,
        gamesViewModel: GamesViewModel,
        driverViewModel: DriverViewModel,
        result: Uri
    ) {
        val userDirectory = DirectoryInitialization.userDirectory
        if (userDirectory == null) {
            Toast.makeText(
                activity.applicationContext,
                R.string.fatal_error,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        ProgressDialogFragment.newInstance(
            activity,
            R.string.importing_user_data
        ) { progressCallback, _ ->
            val checkStream = ZipInputStream(
                BufferedInputStream(activity.contentResolver.openInputStream(result))
            )
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
                    activity,
                    titleId = R.string.invalid_yuzu_backup,
                    descriptionId = R.string.user_data_import_failed_description
                )
            }

            NativeConfig.unloadGlobalConfig()
            File(userDirectory).deleteRecursively()

            try {
                FileUtil.unzipToInternalStorage(
                    result.toString(),
                    File(userDirectory),
                    progressCallback
                )
            } catch (_: Exception) {
                return@newInstance MessageDialogFragment.newInstance(
                    activity,
                    titleId = R.string.import_failed,
                    descriptionId = R.string.user_data_import_failed_description
                )
            }

            NativeLibrary.initializeSystem(true)
            NativeConfig.initializeGlobalConfig()
            gamesViewModel.reloadGames(false)
            driverViewModel.reloadDriverData()

            return@newInstance activity.getString(R.string.user_data_import_success)
        }.show(fragmentManager, ProgressDialogFragment.TAG)
    }
}
