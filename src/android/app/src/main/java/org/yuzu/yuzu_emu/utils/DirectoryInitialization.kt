// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import java.io.IOException
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.YuzuApplication

object DirectoryInitialization {
    private var userPath: String? = null

    var areDirectoriesReady: Boolean = false

    fun start() {
        if (!areDirectoriesReady) {
            initializeInternalStorage()
            NativeLibrary.initializeEmulation()
            areDirectoriesReady = true
        }
    }

    val userDirectory: String?
        get() {
            check(areDirectoriesReady) { "Directory initialization is not ready!" }
            return userPath
        }

    private fun initializeInternalStorage() {
        try {
            userPath = YuzuApplication.appContext.getExternalFilesDir(null)!!.canonicalPath
            NativeLibrary.setAppDirectory(userPath!!)
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }
}
