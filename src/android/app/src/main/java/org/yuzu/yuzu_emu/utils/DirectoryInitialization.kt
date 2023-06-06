// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import org.yuzu.yuzu_emu.NativeLibrary
import java.io.IOException

object DirectoryInitialization {
    private var userPath: String? = null

    var areDirectoriesReady: Boolean = false

    fun start(context: Context) {
        if (!areDirectoriesReady) {
            initializeInternalStorage(context)
            NativeLibrary.initializeEmulation()
            areDirectoriesReady = true
        }
    }

    val userDirectory: String?
        get() {
            check(areDirectoriesReady) { "Directory initialization is not ready!" }
            return userPath
        }

    private fun initializeInternalStorage(context: Context) {
        try {
            userPath = context.getExternalFilesDir(null)!!.canonicalPath
            NativeLibrary.setAppDirectory(userPath!!)
        } catch (e: IOException) {
            e.printStackTrace()
        }
    }
}
