// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import java.io.IOException
import java.nio.charset.StandardCharsets
import java.nio.file.Files
import java.nio.file.Paths
import org.json.JSONException
import org.json.JSONObject

class GpuDriverMetadata(metadataFilePath: String) {
    var name: String? = null
    var description: String? = null
    var author: String? = null
    var vendor: String? = null
    var driverVersion: String? = null
    var minApi = 0
    var libraryName: String? = null

    init {
        try {
            val json = JSONObject(getStringFromFile(metadataFilePath))
            name = json.getString("name")
            description = json.getString("description")
            author = json.getString("author")
            vendor = json.getString("vendor")
            driverVersion = json.getString("driverVersion")
            minApi = json.getInt("minApi")
            libraryName = json.getString("libraryName")
        } catch (e: JSONException) {
            // JSON is malformed, ignore and treat as unsupported metadata.
        } catch (e: IOException) {
            // File is inaccessible, ignore and treat as unsupported metadata.
        }
    }

    companion object {
        @Throws(IOException::class)
        private fun getStringFromFile(filePath: String): String {
            val path = Paths.get(filePath)
            val bytes = Files.readAllBytes(path)
            return String(bytes, StandardCharsets.UTF_8)
        }
    }
}
