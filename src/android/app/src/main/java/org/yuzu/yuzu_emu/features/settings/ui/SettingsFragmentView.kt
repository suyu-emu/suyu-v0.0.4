// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import androidx.fragment.app.FragmentActivity
import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem

/**
 * Abstraction for a screen showing a list of settings. Instances of
 * this type of view will each display a layer of the setting hierarchy.
 */
interface SettingsFragmentView {
    /**
     * Called by the containing Activity to notify the Fragment that an
     * asynchronous load operation completed.
     *
     * @param settings The (possibly null) result of the ini load operation.
     */
    fun onSettingsFileLoaded(settings: Settings)

    /**
     * Pass an ArrayList to the View so that it can be displayed on screen.
     *
     * @param settingsList The result of converting the HashMap to an ArrayList
     */
    fun showSettingsList(settingsList: ArrayList<SettingsItem>)

    /**
     * Called by the containing Activity when an asynchronous load operation fails.
     * Instructs the Fragment to load the settings screen with defaults selected.
     */
    fun loadDefaultSettings()

    /**
     * @return The Fragment's containing activity.
     */
    val fragmentActivity: FragmentActivity

    /**
     * Tell the Fragment to tell the containing Activity to show a new
     * Fragment containing a submenu of settings.
     *
     * @param menuKey Identifier for the settings group that should be shown.
     */
    fun loadSubMenu(menuKey: String)

    /**
     * Tell the Fragment to tell the containing activity to display a toast message.
     *
     * @param message Text to be shown in the Toast
     * @param is_long Whether this should be a long Toast or short one.
     */
    fun showToastMessage(message: String?, is_long: Boolean)

    /**
     * Have the fragment add a setting to the HashMap.
     *
     * @param setting The (possibly previously missing) new setting.
     */
    fun putSetting(setting: Setting)

    /**
     * Have the fragment tell the containing Activity that a setting was modified.
     */
    fun onSettingChanged()
}
