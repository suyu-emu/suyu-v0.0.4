package org.yuzu.yuzu_emu.features.settings.ui

import android.content.IntentFilter
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.utils.DirectoryStateReceiver

/**
 * Abstraction for the Activity that manages SettingsFragments.
 */
interface SettingsActivityView {
    /**
     * Show a new SettingsFragment.
     *
     * @param menuTag    Identifier for the settings group that should be displayed.
     * @param addToStack Whether or not this fragment should replace a previous one.
     */
    fun showSettingsFragment(menuTag: String, addToStack: Boolean, gameId: String)

    /**
     * Called by a contained Fragment to get access to the Setting HashMap
     * loaded from disk, so that each Fragment doesn't need to perform its own
     * read operation.
     *
     * @return A possibly null HashMap of Settings.
     */
    var settings: Settings?

    /**
     * Called when an asynchronous load operation completes.
     *
     * @param settings The (possibly null) result of the ini load operation.
     */
    fun onSettingsFileLoaded(settings: Settings?)

    /**
     * Called when an asynchronous load operation fails.
     */
    fun onSettingsFileNotFound()

    /**
     * Display a popup text message on screen.
     *
     * @param message The contents of the onscreen message.
     * @param is_long Whether this should be a long Toast or short one.
     */
    fun showToastMessage(message: String, is_long: Boolean)

    /**
     * End the activity.
     */
    fun finish()

    /**
     * Called by a containing Fragment to tell the Activity that a setting was changed;
     * unless this has been called, the Activity will not save to disk.
     */
    fun onSettingChanged()

    /**
     * Show loading dialog while loading the settings
     */
    fun showLoading()

    /**
     * Hide the loading the dialog
     */
    fun hideLoading()

    /**
     * Show a hint to the user that the app needs the external storage to be mounted
     */
    fun showExternalStorageNotMountedHint()

    /**
     * Start the DirectoryInitialization and listen for the result.
     *
     * @param receiver the broadcast receiver for the DirectoryInitialization
     * @param filter   the Intent broadcasts to be received.
     */
    fun startDirectoryInitializationService(
        receiver: DirectoryStateReceiver?,
        filter: IntentFilter
    )

    /**
     * Stop listening to the DirectoryInitialization.
     *
     * @param receiver The broadcast receiver to unregister.
     */
    fun stopListeningToDirectoryInitializationService(receiver: DirectoryStateReceiver)
}
