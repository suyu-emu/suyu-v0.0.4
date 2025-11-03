// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.widget.Toast
import androidx.fragment.app.FragmentActivity
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.model.Game
import java.io.File
import kotlin.coroutines.resume
import kotlin.coroutines.suspendCoroutine
import android.net.Uri
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.databinding.DialogProgressBinding
import android.view.LayoutInflater
import kotlinx.coroutines.channels.Channel
import kotlinx.coroutines.launch
import kotlinx.coroutines.CoroutineScope

object CustomSettingsHandler {
    const val CUSTOM_CONFIG_ACTION = "dev.eden.eden_emulator.LAUNCH_WITH_CUSTOM_CONFIG"
    const val EXTRA_TITLE_ID = "title_id"
    const val EXTRA_CUSTOM_SETTINGS = "custom_settings"

    /**
     * Apply custom settings from a string instead of loading from file
     * @param titleId The game title ID (16-digit hex string)
     * @param customSettings The complete INI file content as string
     * @param context Application context
     * @return Game object created from title ID, or null if not found
     */
    fun applyCustomSettings(titleId: String, customSettings: String, context: Context): Game? {
        // For synchronous calls without driver checking
        Log.info("[CustomSettingsHandler] Applying custom settings for title ID: $titleId")
        // Find the game by title ID
        val game = findGameByTitleId(titleId, context)
        if (game == null) {
            Log.error("[CustomSettingsHandler] Game not found for title ID: $titleId")
            return null
        }

        // Check if config already exists - this should be handled by the caller
        val configFile = getConfigFile(game)
        if (configFile.exists()) {
            Log.warning(
                "[CustomSettingsHandler] Config file already exists for game: ${game.title}"
            )
        }

        // Write the config file
        if (!writeConfigFile(game, customSettings)) {
            Log.error("[CustomSettingsHandler] Failed to write config file")
            return null
        }

        // Initialize per-game config
        try {
            val fileName = FileUtil.getFilename(Uri.parse(game.path))
            NativeConfig.initializePerGameConfig(game.programId, fileName)
            Log.info("[CustomSettingsHandler] Successfully applied custom settings")
            return game
        } catch (e: Exception) {
            Log.error("[CustomSettingsHandler] Failed to apply custom settings: ${e.message}")
            return null
        }
    }

    /**
     * Apply custom settings with automatic driver checking and installation
     * @param titleId The game title ID (16-digit hex string)
     * @param customSettings The complete INI file content as string
     * @param context Application context
     * @param activity Fragment activity for driver installation dialogs (optional)
     * @param driverViewModel DriverViewModel for driver management (optional)
     * @return Game object created from title ID, or null if not found
     */
    suspend fun applyCustomSettingsWithDriverCheck(
        titleId: String,
        customSettings: String,
        context: Context,
        activity: FragmentActivity?,
        driverViewModel: DriverViewModel?
    ): Game? {
        Log.info("[CustomSettingsHandler] Applying custom settings for title ID: $titleId")
        // Find the game by title ID
        val game = findGameByTitleId(titleId, context)
        if (game == null) {
            Log.error("[CustomSettingsHandler] Game not found for title ID: $titleId")
            // This will be handled by the caller to show appropriate error message
            return null
        }

        // Check if config already exists
        val configFile = getConfigFile(game)
        if (configFile.exists() && activity != null) {
            Log.info(
                "[CustomSettingsHandler] Config file already exists, asking user for confirmation"
            )
            Toast.makeText(
                activity,
                activity.getString(R.string.config_exists_prompt),
                Toast.LENGTH_SHORT
            ).show()
            val shouldOverwrite = askUserToOverwriteConfig(activity, game.title)
            if (!shouldOverwrite) {
                Log.info("[CustomSettingsHandler] User chose not to overwrite existing config")
                Toast.makeText(
                    activity,
                    activity.getString(R.string.overwrite_cancelled),
                    Toast.LENGTH_SHORT
                ).show()
                return null
            }
        }

        // Check for driver requirements if activity and driverViewModel are provided
        if (activity != null && driverViewModel != null) {
            val rawDriverPath = extractDriverPath(customSettings)
            if (rawDriverPath != null) {
                // Normalize to local storage path (we only store drivers under driverStoragePath)
                val driverFilename = rawDriverPath.substringAfterLast('/')
                    .substringAfterLast('\\')
                val localDriverPath = "${GpuDriverHelper.driverStoragePath}$driverFilename"
                Log.info("[CustomSettingsHandler] Custom settings specify driver: $rawDriverPath (normalized: $localDriverPath)")

                // Check if driver exists in the driver storage
                val driverFile = File(localDriverPath)
                if (!driverFile.exists()) {
                    Log.info("[CustomSettingsHandler] Driver not found locally: ${driverFile.name}")

                    // Ask user if they want to download the missing driver
                    val shouldDownload = askUserToDownloadDriver(activity, driverFile.name)
                    if (!shouldDownload) {
                        Log.info("[CustomSettingsHandler] User declined to download driver")
                        Toast.makeText(
                            activity,
                            activity.getString(R.string.driver_download_cancelled),
                            Toast.LENGTH_SHORT
                        ).show()
                        return null
                    }

                    // Check network connectivity after user consent
                    if (!DriverResolver.isNetworkAvailable(activity)) {
                        Log.error("[CustomSettingsHandler] No network connection available")
                        Toast.makeText(
                            activity,
                            activity.getString(R.string.network_unavailable),
                            Toast.LENGTH_LONG
                        ).show()
                        return null
                    }

                    Log.info("[CustomSettingsHandler] User approved, downloading driver")

                    // Show progress dialog for driver download
                    val dialogBinding = DialogProgressBinding.inflate(LayoutInflater.from(activity))
                    dialogBinding.progressBar.isIndeterminate = false
                    dialogBinding.title.text = activity.getString(R.string.installing_driver)
                    dialogBinding.status.text = activity.getString(R.string.downloading)

                    val progressDialog = MaterialAlertDialogBuilder(activity)
                        .setView(dialogBinding.root)
                        .setCancelable(false)
                        .create()

                    withContext(Dispatchers.Main) {
                        progressDialog.show()
                    }

                    try {
                        // Set up progress channel for thread-safe UI updates
                        val progressChannel = Channel<Int>(Channel.CONFLATED)
                        val progressJob = CoroutineScope(Dispatchers.Main).launch {
                            for (progress in progressChannel) {
                                dialogBinding.progressBar.progress = progress
                            }
                        }

                        // Attempt to download and install the driver
                        val driverUri = DriverResolver.ensureDriverAvailable(driverFilename, activity) { progress ->
                            progressChannel.trySend(progress.toInt())
                        }

                        progressChannel.close()
                        progressJob.cancel()

                        withContext(Dispatchers.Main) {
                            progressDialog.dismiss()
                        }

                        if (driverUri == null) {
                            Log.error(
                                "[CustomSettingsHandler] Failed to download driver: ${driverFile.name}"
                            )
                            Toast.makeText(
                                activity,
                                activity.getString(
                                    R.string.custom_settings_failed_message,
                                    game.title,
                                    activity.getString(R.string.driver_not_found, driverFile.name)
                                ),
                                Toast.LENGTH_LONG
                            ).show()
                            return null
                        }

                        // Verify the downloaded driver (from normalized local path)
                        val installedFile = File(localDriverPath)
                        val metadata = GpuDriverHelper.getMetadataFromZip(installedFile)
                        if (metadata.name == null) {
                            Log.error(
                                "[CustomSettingsHandler] Downloaded driver is invalid: $localDriverPath"
                            )
                            Toast.makeText(
                                activity,
                                activity.getString(
                                    R.string.custom_settings_failed_message,
                                    game.title,
                                    activity.getString(
                                        R.string.invalid_driver_file,
                                        driverFile.name
                                    )
                                ),
                                Toast.LENGTH_LONG
                            ).show()
                            return null
                        }

                        // Add to driver list
                        driverViewModel.onDriverAdded(Pair(localDriverPath, metadata))
                        Log.info(
                            "[CustomSettingsHandler] Successfully downloaded and installed driver: ${metadata.name}"
                        )

                        Toast.makeText(
                            activity,
                            activity.getString(
                                R.string.successfully_installed,
                                metadata.name ?: driverFile.name
                            ),
                            Toast.LENGTH_SHORT
                        ).show()
                    } catch (e: Exception) {
                        withContext(Dispatchers.Main) {
                            progressDialog.dismiss()
                        }
                        Log.error("[CustomSettingsHandler] Error downloading driver: ${e.message}")
                        Toast.makeText(
                            activity,
                            activity.getString(
                                R.string.custom_settings_failed_message,
                                game.title,
                                e.message ?: activity.getString(
                                    R.string.driver_not_found,
                                    driverFile.name
                                )
                            ),
                            Toast.LENGTH_LONG
                        ).show()
                        return null
                    }
                } else {
                    // Driver exists, verify it's valid
                    val metadata = GpuDriverHelper.getMetadataFromZip(driverFile)
                    if (metadata.name == null) {
                        Log.error("[CustomSettingsHandler] Invalid driver file: $localDriverPath")
                        Toast.makeText(
                            activity,
                            activity.getString(
                                R.string.custom_settings_failed_message,
                                game.title,
                                activity.getString(R.string.invalid_driver_file, driverFile.name)
                            ),
                            Toast.LENGTH_LONG
                        ).show()
                        return null
                    }
                    Log.info("[CustomSettingsHandler] Driver verified: ${metadata.name}")
                }
            }
        }

        // Only write the config file after all checks pass
        if (!writeConfigFile(game, customSettings)) {
            Log.error("[CustomSettingsHandler] Failed to write config file")
            activity?.let {
                Toast.makeText(
                    it,
                    it.getString(R.string.config_write_failed),
                    Toast.LENGTH_SHORT
                ).show()
            }
            return null
        }

        // Initialize per-game config
        try {
            SettingsFile.loadCustomConfig(game)
            Log.info("[CustomSettingsHandler] Successfully applied custom settings")
            activity?.let {
                Toast.makeText(
                    it,
                    it.getString(R.string.custom_settings_applied),
                    Toast.LENGTH_SHORT
                ).show()
            }
            return game
        } catch (e: Exception) {
            Log.error("[CustomSettingsHandler] Failed to apply custom settings: ${e.message}")
            activity?.let {
                Toast.makeText(
                    it,
                    it.getString(R.string.config_apply_failed),
                    Toast.LENGTH_SHORT
                ).show()
            }
            return null
        }
    }

    /**
     * Find a game by its title ID in the user's game library
     */
    fun findGameByTitleId(titleId: String, context: Context): Game? {
        Log.info("[CustomSettingsHandler] Searching for game with title ID: $titleId")
        // Convert hex title ID to decimal for comparison with programId
        val programIdDecimal = try {
            titleId.toLong(16).toString()
        } catch (e: NumberFormatException) {
            Log.error("[CustomSettingsHandler] Invalid title ID format: $titleId")
            return null
        }

        // Expected hex format with "0" prefix
        val expectedHex = "0${titleId.uppercase()}"
        // First check cached games for fast lookup
        GameHelper.cachedGameList.find { game ->
            game.programId == programIdDecimal ||
                game.programIdHex.equals(expectedHex, ignoreCase = true)
        }?.let { foundGame ->
            Log.info("[CustomSettingsHandler] Found game in cache: ${foundGame.title}")
            return foundGame
        }
        // If not in cache, perform full game library scan
        Log.info("[CustomSettingsHandler] Game not in cache, scanning full library...")
        val allGames = GameHelper.getGames()
        val foundGame = allGames.find { game ->
            game.programId == programIdDecimal ||
                game.programIdHex.equals(expectedHex, ignoreCase = true)
        }
        if (foundGame != null) {
            Log.info("[CustomSettingsHandler] Found game: ${foundGame.title} at ${foundGame.path}")
        } else {
            Log.warning("[CustomSettingsHandler] No game found for title ID: $titleId")
        }
        return foundGame
    }

    /**
     * Get the config file path for a game
     */
    private fun getConfigFile(game: Game): File {
        return SettingsFile.getCustomSettingsFile(game)
    }

    /**
     * Get the title ID config file path
     */
    private fun getTitleIdConfigFile(titleId: String): File {
        val configDir = File(DirectoryInitialization.userDirectory, "config/custom")
        return File(configDir, "$titleId.ini")
    }

    /**
     * Write the config file with the custom settings
     */
    private fun writeConfigFile(game: Game, customSettings: String): Boolean {
        return try {
            val configFile = getConfigFile(game)
            val configDir = configFile.parentFile
            if (configDir != null && !configDir.exists()) {
                configDir.mkdirs()
            }

            configFile.writeText(customSettings)

            Log.info("[CustomSettingsHandler] Wrote config file: ${configFile.absolutePath}")
            true
        } catch (e: Exception) {
            Log.error("[CustomSettingsHandler] Failed to write config file: ${e.message}")
            false
        }
    }

    /**
     * Ask user if they want to overwrite existing configuration
     */
    private suspend fun askUserToOverwriteConfig(activity: FragmentActivity, gameTitle: String): Boolean {
        return suspendCoroutine { continuation ->
            activity.runOnUiThread {
                MaterialAlertDialogBuilder(activity)
                    .setTitle(activity.getString(R.string.config_already_exists_title))
                    .setMessage(
                        activity.getString(R.string.config_already_exists_message, gameTitle)
                    )
                    .setPositiveButton(activity.getString(R.string.overwrite)) { _, _ ->
                        continuation.resume(true)
                    }
                    .setNegativeButton(activity.getString(R.string.cancel)) { _, _ ->
                        continuation.resume(false)
                    }
                    .setCancelable(false)
                    .show()
            }
        }
    }

    /**
     * Ask user if they want to download a missing driver
     */
    private suspend fun askUserToDownloadDriver(activity: FragmentActivity, driverName: String): Boolean {
        return suspendCoroutine { continuation ->
            activity.runOnUiThread {
                MaterialAlertDialogBuilder(activity)
                    .setTitle(activity.getString(R.string.driver_missing_title))
                    .setMessage(
                        activity.getString(R.string.driver_missing_message, driverName)
                    )
                    .setPositiveButton(activity.getString(R.string.download)) { _, _ ->
                        continuation.resume(true)
                    }
                    .setNegativeButton(activity.getString(R.string.cancel)) { _, _ ->
                        continuation.resume(false)
                    }
                    .setCancelable(false)
                    .show()
            }
        }
    }

    /**
     * Extract driver path from custom settings INI content
     */
    private fun extractDriverPath(customSettings: String): String? {
        val lines = customSettings.lines()
        var inGpuDriverSection = false

        for (line in lines) {
            val trimmed = line.trim()
            if (trimmed.startsWith("[") && trimmed.endsWith("]")) {
                inGpuDriverSection = trimmed == "[GpuDriver]"
                continue
            }

            if (inGpuDriverSection && trimmed.startsWith("driver_path=")) {
                return trimmed.substringAfter("driver_path=")
                    .trim()
                    .removeSurrounding("\"", "\"")
            }
        }

        return null
    }
}
