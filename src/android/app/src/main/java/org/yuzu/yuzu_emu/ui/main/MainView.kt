// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.main

/**
 * Abstraction for the screen that shows on application launch.
 * Implementations will differ primarily to target touch-screen
 * or non-touch screen devices.
 */
interface MainView {
    /**
     * Pass the view the native library's version string. Displaying
     * it is optional.
     *
     * @param version A string pulled from native code.
     */
    fun setVersionString(version: String)

    /**
     * Tell the view to refresh its contents.
     */
    fun refresh()
    fun launchSettingsActivity(menuTag: String)
    fun launchFileListActivity(request: Int)
}
