// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.ui.main

import org.yuzu.yuzu_emu.BuildConfig
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile

class MainPresenter(private val view: MainView) {
    fun onCreate() {
        val versionName = BuildConfig.VERSION_NAME
        view.setVersionString(versionName)
    }

    private fun launchFileListActivity(request: Int) {
        view.launchFileListActivity(request)
    }

    fun handleOptionSelection(itemId: Int): Boolean {
        when (itemId) {
            R.id.menu_settings_core -> {
                view.launchSettingsActivity(SettingsFile.FILE_NAME_CONFIG)
                return true
            }
            R.id.button_add_directory -> {
                launchFileListActivity(REQUEST_ADD_DIRECTORY)
                return true
            }
            R.id.button_install_keys -> {
                launchFileListActivity(REQUEST_INSTALL_KEYS)
                return true
            }
            R.id.button_install_amiibo_keys -> {
                launchFileListActivity(REQUEST_INSTALL_AMIIBO_KEYS)
                return true
            }
            R.id.button_select_gpu_driver -> {
                launchFileListActivity(REQUEST_SELECT_GPU_DRIVER)
                return true
            }
        }
        return false
    }

    companion object {
        const val REQUEST_ADD_DIRECTORY = 1
        const val REQUEST_INSTALL_KEYS = 2
        const val REQUEST_INSTALL_AMIIBO_KEYS = 3
        const val REQUEST_SELECT_GPU_DRIVER = 4
    }
}
