// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import org.yuzu.yuzu_emu.features.settings.model.Settings

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
     * @return A HashMap of Settings.
     */
    val settings: Settings

    /**
     * Called when a load operation completes.
     */
    fun onSettingsFileLoaded()

    /**
     * Called when a load operation fails.
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
}
