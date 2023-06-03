// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.utils

import android.content.Context
import android.database.Cursor
import android.net.Uri
import android.provider.DocumentsContract
import androidx.documentfile.provider.DocumentFile
import org.yuzu.yuzu_emu.model.MinimalDocumentFile
import java.io.BufferedInputStream
import java.io.File
import java.io.FileOutputStream
import java.io.IOException
import java.io.InputStream
import java.net.URLDecoder
import java.util.zip.ZipEntry
import java.util.zip.ZipInputStream

object FileUtil {
    const val PATH_TREE = "tree"
    const val DECODE_METHOD = "UTF-8"
    const val APPLICATION_OCTET_STREAM = "application/octet-stream"
    const val TEXT_PLAIN = "text/plain"

    /**
     * Create a file from directory with filename.
     * @param context Application context
     * @param directory parent path for file.
     * @param filename file display name.
     * @return boolean
     */
    fun createFile(context: Context?, directory: String?, filename: String): DocumentFile? {
        var decodedFilename = filename
        try {
            val directoryUri = Uri.parse(directory)
            val parent = DocumentFile.fromTreeUri(context!!, directoryUri) ?: return null
            decodedFilename = URLDecoder.decode(decodedFilename, DECODE_METHOD)
            var mimeType = APPLICATION_OCTET_STREAM
            if (decodedFilename.endsWith(".txt")) {
                mimeType = TEXT_PLAIN
            }
            val exists = parent.findFile(decodedFilename)
            return exists ?: parent.createFile(mimeType, decodedFilename)
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot create file, error: " + e.message)
        }
        return null
    }

    /**
     * Create a directory from directory with filename.
     * @param context Application context
     * @param directory parent path for directory.
     * @param directoryName directory display name.
     * @return boolean
     */
    fun createDir(context: Context?, directory: String?, directoryName: String?): DocumentFile? {
        var decodedDirectoryName = directoryName
        try {
            val directoryUri = Uri.parse(directory)
            val parent = DocumentFile.fromTreeUri(context!!, directoryUri) ?: return null
            decodedDirectoryName = URLDecoder.decode(decodedDirectoryName, DECODE_METHOD)
            val isExist = parent.findFile(decodedDirectoryName)
            return isExist ?: parent.createDirectory(decodedDirectoryName)
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot create file, error: " + e.message)
        }
        return null
    }

    /**
     * Open content uri and return file descriptor to JNI.
     * @param context Application context
     * @param path Native content uri path
     * @param openMode will be one of "r", "r", "rw", "wa", "rwa"
     * @return file descriptor
     */
    @JvmStatic
    fun openContentUri(context: Context, path: String, openMode: String?): Int {
        try {
            val uri = Uri.parse(path)
            val parcelFileDescriptor = context.contentResolver.openFileDescriptor(uri, openMode!!)
            if (parcelFileDescriptor == null) {
                Log.error("[FileUtil]: Cannot get the file descriptor from uri: $path")
                return -1
            }
            val fileDescriptor = parcelFileDescriptor.detachFd()
            parcelFileDescriptor.close()
            return fileDescriptor
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot open content uri, error: " + e.message)
        }
        return -1
    }

    /**
     * Reference:  https://stackoverflow.com/questions/42186820/documentfile-is-very-slow
     * This function will be faster than DoucmentFile.listFiles
     * @param context Application context
     * @param uri Directory uri.
     * @return CheapDocument lists.
     */
    fun listFiles(context: Context, uri: Uri): Array<MinimalDocumentFile> {
        val resolver = context.contentResolver
        val columns = arrayOf(
            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
            DocumentsContract.Document.COLUMN_MIME_TYPE
        )
        var c: Cursor? = null
        val results: MutableList<MinimalDocumentFile> = ArrayList()
        try {
            val docId: String = if (isRootTreeUri(uri)) {
                DocumentsContract.getTreeDocumentId(uri)
            } else {
                DocumentsContract.getDocumentId(uri)
            }
            val childrenUri = DocumentsContract.buildChildDocumentsUriUsingTree(uri, docId)
            c = resolver.query(childrenUri, columns, null, null, null)
            while (c!!.moveToNext()) {
                val documentId = c.getString(0)
                val documentName = c.getString(1)
                val documentMimeType = c.getString(2)
                val documentUri = DocumentsContract.buildDocumentUriUsingTree(uri, documentId)
                val document = MinimalDocumentFile(documentName, documentMimeType, documentUri)
                results.add(document)
            }
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot list file error: " + e.message)
        } finally {
            closeQuietly(c)
        }
        return results.toTypedArray()
    }

    /**
     * Check whether given path exists.
     * @param path Native content uri path
     * @return bool
     */
    fun exists(context: Context, path: String?): Boolean {
        var c: Cursor? = null
        try {
            val mUri = Uri.parse(path)
            val columns = arrayOf(DocumentsContract.Document.COLUMN_DOCUMENT_ID)
            c = context.contentResolver.query(mUri, columns, null, null, null)
            return c!!.count > 0
        } catch (e: Exception) {
            Log.info("[FileUtil] Cannot find file from given path, error: " + e.message)
        } finally {
            closeQuietly(c)
        }
        return false
    }

    /**
     * Check whether given path is a directory
     * @param path content uri path
     * @return bool
     */
    fun isDirectory(context: Context, path: String): Boolean {
        val resolver = context.contentResolver
        val columns = arrayOf(
            DocumentsContract.Document.COLUMN_MIME_TYPE
        )
        var isDirectory = false
        var c: Cursor? = null
        try {
            val mUri = Uri.parse(path)
            c = resolver.query(mUri, columns, null, null, null)
            c!!.moveToNext()
            val mimeType = c.getString(0)
            isDirectory = mimeType == DocumentsContract.Document.MIME_TYPE_DIR
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot list files, error: " + e.message)
        } finally {
            closeQuietly(c)
        }
        return isDirectory
    }

    /**
     * Get file display name from given path
     * @param path content uri path
     * @return String display name
     */
    fun getFilename(context: Context, path: String): String {
        val resolver = context.contentResolver
        val columns = arrayOf(
            DocumentsContract.Document.COLUMN_DISPLAY_NAME
        )
        var filename = ""
        var c: Cursor? = null
        try {
            val mUri = Uri.parse(path)
            c = resolver.query(mUri, columns, null, null, null)
            c!!.moveToNext()
            filename = c.getString(0)
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot get file size, error: " + e.message)
        } finally {
            closeQuietly(c)
        }
        return filename
    }

    fun getFilesName(context: Context, path: String): Array<String> {
        val uri = Uri.parse(path)
        val files: MutableList<String> = ArrayList()
        for (file in listFiles(context, uri)) {
            files.add(file.filename)
        }
        return files.toTypedArray()
    }

    /**
     * Get file size from given path.
     * @param path content uri path
     * @return long file size
     */
    @JvmStatic
    fun getFileSize(context: Context, path: String): Long {
        val resolver = context.contentResolver
        val columns = arrayOf(
            DocumentsContract.Document.COLUMN_SIZE
        )
        var size: Long = 0
        var c: Cursor? = null
        try {
            val mUri = Uri.parse(path)
            c = resolver.query(mUri, columns, null, null, null)
            c!!.moveToNext()
            size = c.getLong(0)
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot get file size, error: " + e.message)
        } finally {
            closeQuietly(c)
        }
        return size
    }

    fun copyUriToInternalStorage(
        context: Context,
        sourceUri: Uri?,
        destinationParentPath: String,
        destinationFilename: String
    ): Boolean {
        var input: InputStream? = null
        var output: FileOutputStream? = null
        try {
            input = context.contentResolver.openInputStream(sourceUri!!)
            output = FileOutputStream("$destinationParentPath/$destinationFilename")
            val buffer = ByteArray(1024)
            var len: Int
            while (input!!.read(buffer).also { len = it } != -1) {
                output.write(buffer, 0, len)
            }
            output.flush()
            return true
        } catch (e: Exception) {
            Log.error("[FileUtil]: Cannot copy file, error: " + e.message)
        } finally {
            if (input != null) {
                try {
                    input.close()
                } catch (e: IOException) {
                    Log.error("[FileUtil]: Cannot close input file, error: " + e.message)
                }
            }
            if (output != null) {
                try {
                    output.close()
                } catch (e: IOException) {
                    Log.error("[FileUtil]: Cannot close output file, error: " + e.message)
                }
            }
        }
        return false
    }

    /**
     * Extracts the given zip file into the given directory.
     * @exception IOException if the file was being created outside of the target directory
     */
    @Throws(SecurityException::class)
    fun unzip(zipStream: InputStream, destDir: File): Boolean {
        ZipInputStream(BufferedInputStream(zipStream)).use { zis ->
            var entry: ZipEntry? = zis.nextEntry
            while (entry != null) {
                val entryName = entry.name
                val entryFile = File(destDir, entryName)
                if (!entryFile.canonicalPath.startsWith(destDir.canonicalPath + File.separator)) {
                    throw SecurityException("Entry is outside of the target dir: " + entryFile.name)
                }
                if (entry.isDirectory) {
                    entryFile.mkdirs()
                } else {
                    entryFile.parentFile?.mkdirs()
                    entryFile.createNewFile()
                    entryFile.outputStream().use { fos -> zis.copyTo(fos) }
                }
                entry = zis.nextEntry
            }
        }

        return true
    }

    fun isRootTreeUri(uri: Uri): Boolean {
        val paths = uri.pathSegments
        return paths.size == 2 && PATH_TREE == paths[0]
    }

    fun closeQuietly(closeable: AutoCloseable?) {
        if (closeable != null) {
            try {
                closeable.close()
            } catch (rethrown: RuntimeException) {
                throw rethrown
            } catch (ignored: Exception) {
            }
        }
    }

    fun hasExtension(path: String, extension: String): Boolean {
        return path.substring(path.lastIndexOf(".") + 1).contains(extension)
    }
}
