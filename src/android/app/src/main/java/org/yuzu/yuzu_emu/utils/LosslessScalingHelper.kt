// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

import android.net.Uri
import kotlinx.coroutines.flow.MutableStateFlow
import kotlinx.coroutines.flow.StateFlow
import kotlinx.coroutines.flow.asStateFlow
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import java.io.File

object LosslessScalingHelper {
    const val RESULT_OK = 0
    const val RESULT_NOT_INSTALLED = 1

    private val _statusText = MutableStateFlow("")
    val statusText: StateFlow<String> = _statusText.asStateFlow()

    private var installed: Boolean? = null
    private var gpuSupported: Boolean? = null

    fun isInstalled(): Boolean = installed ?: refreshStatus()

    fun isSupportedByGpu(): Boolean {
        val cached = gpuSupported
        if (cached != null) {
            return cached
        }
        val result = NativeLibrary.supportsFrameGeneration()
        gpuSupported = result
        return result
    }

    fun refreshStatus(): Boolean {
        val result = NativeLibrary.validateLosslessDll() == RESULT_OK
        installed = result

        val context = YuzuApplication.appContext
        _statusText.value = if (result) {
            context.getString(R.string.lossless_scaling_installed)
        } else {
            context.getString(R.string.lossless_scaling_not_installed)
        }
        return result
    }

    fun install(source: Uri): Int {
        val destination = File(NativeLibrary.getLosslessDllPath())
        destination.parentFile?.mkdirs()

        val copied = FileUtil.copyUriToInternalStorage(
            source,
            destination.parent!!,
            destination.name
        )
        if (copied == null) {
            refreshStatus()
            return RESULT_NOT_INSTALLED
        }

        val result = NativeLibrary.prepareLosslessDll()
        if (result != RESULT_OK) {
            NativeLibrary.removeLosslessDll()
        }
        refreshStatus()
        return result
    }

    fun remove(): Boolean {
        val removed = NativeLibrary.removeLosslessDll()
        refreshStatus()
        return removed
    }
}
