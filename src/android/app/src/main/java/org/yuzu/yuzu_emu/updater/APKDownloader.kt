// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.updater

import okhttp3.Call
import okhttp3.Callback
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import java.io.File
import java.io.FileOutputStream
import java.io.IOException

class APKDownloader(private val url: String, private val outputFile: File) {

    fun download(onProgress: (Int) -> Unit, onComplete: (Boolean) -> Unit) {
        val client = OkHttpClient()
        val request = Request.Builder().url(url).build()

        client.newCall(request).enqueue(object : Callback {
            override fun onFailure(call: Call, e: IOException) {
                e.printStackTrace()
                onComplete(false)
            }

            override fun onResponse(call: Call, response: Response) {
                if (response.isSuccessful) {
                    response.body?.let { body ->
                        val contentLength = body.contentLength()
                        try {
                            val inputStream = body.byteStream()
                            val outputStream = FileOutputStream(outputFile)
                            val buffer = ByteArray(4096)
                            var bytesRead: Int
                            var totalBytesRead: Long = 0

                            while (inputStream.read(buffer).also { bytesRead = it } != -1) {
                                outputStream.write(buffer, 0, bytesRead)
                                totalBytesRead += bytesRead
                                val progress = (totalBytesRead * 100 / contentLength).toInt()
                                onProgress(progress)
                            }
                            outputStream.flush()
                            outputStream.close()
                            inputStream.close()
                            onComplete(true)
                        } catch (e: IOException) {
                            e.printStackTrace()
                            onComplete(false)
                        }
                    } ?: run {
                        onComplete(false)
                    }
                } else {
                    onComplete(false)
                }
            }
        })
    }
}
