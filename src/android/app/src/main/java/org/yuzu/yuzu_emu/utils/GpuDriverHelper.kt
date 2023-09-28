// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.net.Uri
import java.io.BufferedInputStream
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.io.IOException
import java.util.zip.ZipInputStream
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.utils.FileUtil.copyUriToInternalStorage
import org.yuzu.yuzu_emu.YuzuApplication

object GpuDriverHelper {
    private const val META_JSON_FILENAME = "meta.json"
    private const val DRIVER_INTERNAL_FILENAME = "gpu_driver.zip"
    private var fileRedirectionPath: String? = null
    private var driverInstallationPath: String? = null
    private var hookLibPath: String? = null

    @Throws(IOException::class)
    private fun unzip(zipFilePath: String, destDir: String) {
        val dir = File(destDir)

        // Create output directory if it doesn't exist
        if (!dir.exists()) dir.mkdirs()

        // Unpack the files.
        val inputStream = FileInputStream(zipFilePath)
        val zis = ZipInputStream(BufferedInputStream(inputStream))
        val buffer = ByteArray(1024)
        var ze = zis.nextEntry
        while (ze != null) {
            val newFile = File(destDir, ze.name)
            val canonicalPath = newFile.canonicalPath
            if (!canonicalPath.startsWith(destDir + ze.name)) {
                throw SecurityException("Zip file attempted path traversal! " + ze.name)
            }

            newFile.parentFile!!.mkdirs()
            val fos = FileOutputStream(newFile)
            var len: Int
            while (zis.read(buffer).also { len = it } > 0) {
                fos.write(buffer, 0, len)
            }
            fos.close()
            zis.closeEntry()
            ze = zis.nextEntry
        }
        zis.closeEntry()
    }

    fun initializeDriverParameters(context: Context) {
        try {
            // Initialize the file redirection directory.
            fileRedirectionPath =
                context.getExternalFilesDir(null)!!.canonicalPath + "/gpu/vk_file_redirect/"

            // Initialize the driver installation directory.
            driverInstallationPath = context.filesDir.canonicalPath + "/gpu_driver/"
                .filesDir.canonicalPath + "/gpu_driver/"
        } catch (e: IOException) {
            throw RuntimeException(e)
        }

        // Initialize directories.
        initializeDirectories()

        // Initialize hook libraries directory.
        hookLibPath = context.applicationInfo.nativeLibraryDir + "/"
        hookLibPath = YuzuApplication.appContext.applicationInfo.nativeLibraryDir + "/"

        // Initialize GPU driver.
        NativeLibrary.initializeGpuDriver(
            hookLibPath,
            driverInstallationPath,
            customDriverLibraryName,
            fileRedirectionPath
        )
    }

    fun installDefaultDriver(context: Context) {
    fun installDefaultDriver() {
        // Removing the installed driver will result in the backend using the default system driver.
        val driverInstallationDir = File(driverInstallationPath!!)
        deleteRecursive(driverInstallationDir)
    }

    fun installCustomDriver(context: Context, driverPathUri: Uri?) {
        // Revert to system default in the event the specified driver is bad.
        installDefaultDriver()

        // Ensure we have directories.
        initializeDirectories()

        // Copy the zip file URI into our private storage.
        copyUriToInternalStorage(
            context,
            driverPathUri,
            driverInstallationPath!!,
            DRIVER_INTERNAL_FILENAME
        )

        // Unzip the driver.
        try {
            unzip(driverInstallationPath + DRIVER_INTERNAL_FILENAME, driverInstallationPath!!)
        } catch (e: SecurityException) {
            return
        }

        // Initialize the driver parameters.
        initializeDriverParameters(context)
    }

    external fun supportsCustomDriverLoading(): Boolean

    // Parse the custom driver metadata to retrieve the name.
    val customDriverName: String?
        get() {
            val metadata = GpuDriverMetadata(driverInstallationPath + META_JSON_FILENAME)
            return metadata.name
        }

    // Parse the custom driver metadata to retrieve the library name.
    private val customDriverLibraryName: String?
        get() {
            // Parse the custom driver metadata to retrieve the library name.
            val metadata = GpuDriverMetadata(driverInstallationPath + META_JSON_FILENAME)
            return metadata.libraryName
        }

    private fun initializeDirectories() {
        // Ensure the file redirection directory exists.
        val fileRedirectionDir = File(fileRedirectionPath!!)
        if (!fileRedirectionDir.exists()) {
            fileRedirectionDir.mkdirs()
        }
        // Ensure the driver installation directory exists.
        val driverInstallationDir = File(driverInstallationPath!!)
        if (!driverInstallationDir.exists()) {
            driverInstallationDir.mkdirs()
        }
    }

    private fun deleteRecursive(fileOrDirectory: File) {
        if (fileOrDirectory.isDirectory) {
            for (child in fileOrDirectory.listFiles()!!) {
                deleteRecursive(child)
            }
        }
        fileOrDirectory.delete()
    }
}
