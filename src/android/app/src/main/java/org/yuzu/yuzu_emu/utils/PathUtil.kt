// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.net.Uri
import android.provider.DocumentsContract
import java.io.File

object PathUtil {

    /**
     * Converts a content:// URI from the Storage Access Framework to a real filesystem path.
     */
    fun getPathFromUri(uri: Uri): String? {
        val docId = try {
            DocumentsContract.getTreeDocumentId(uri)
        } catch (_: Exception) {
            return null
        }

        if (docId.startsWith("primary:")) {
            val relativePath = docId.substringAfter(":")
            val primaryStoragePath = android.os.Environment.getExternalStorageDirectory().absolutePath
            return "$primaryStoragePath/$relativePath"
        }

        // external SD cards and other volumes)
        val storageIdString = docId.substringBefore(":")
        val removablePath = getRemovableStoragePath(storageIdString)
        if (removablePath != null) {
            return "$removablePath/${docId.substringAfter(":")}"
        }

        return null
    }

    /**
     * Validates that a path is a valid, writable directory.
     * Creates the directory if it doesn't exist.
     */
    fun validateDirectory(path: String): Boolean {
        val dir = File(path)

        if (!dir.exists()) {
            if (!dir.mkdirs()) {
                return false
            }
        }

        return dir.isDirectory && dir.canWrite()
    }

    /**
     * Copies a directory recursively from source to destination.
     */
    fun copyDirectory(source: File, destination: File, overwrite: Boolean = true): Boolean {
        return try {
            source.copyRecursively(destination, overwrite)
            true
        } catch (_: Exception) {
            false
        }
    }

    /**
     * Checks if a directory has any content.
     */
    fun hasContent(path: String): Boolean {
        val dir = File(path)
        return dir.exists() && dir.listFiles()?.isNotEmpty() == true
    }


    fun truncatePathForDisplay(path: String, maxLength: Int = 40): String {
        return if (path.length > maxLength) {
            "...${path.takeLast(maxLength - 3)}"
        } else {
            path
        }
    }

    // This really shouldn't be necessary, but the Android API seemingly
    // doesn't have a way of doing this?
    // Apparently, on certain devices the mount location can vary, so add
    // extra cases here if we discover any new ones.
    fun getRemovableStoragePath(idString: String): String? {
        var pathFile: File

        pathFile = File("/mnt/media_rw/$idString");
        if (pathFile.exists()) {
            return pathFile.absolutePath
        }

        return null
    }
}
