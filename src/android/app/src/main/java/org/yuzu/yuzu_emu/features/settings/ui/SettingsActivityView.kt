// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

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
     * Called when a load operation completes.
     */
    fun onSettingsFileLoaded()

    /**
     * Called when a load operation fails.
     */
    fun onSettingsFileNotFound()

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
