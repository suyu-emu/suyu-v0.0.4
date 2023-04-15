// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.disk_shader_cache

import androidx.annotation.Keep
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.disk_shader_cache.ui.ShaderProgressDialogFragment

@Keep
object DiskShaderCacheProgress {
    val finishLock = Object()
    private lateinit var fragment: ShaderProgressDialogFragment

    private fun prepareDialog() {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()!!
        emulationActivity.runOnUiThread {
            fragment = ShaderProgressDialogFragment.newInstance(
                emulationActivity.getString(R.string.loading),
                emulationActivity.getString(R.string.preparing_shaders)
            )
            fragment.show(emulationActivity.supportFragmentManager, ShaderProgressDialogFragment.TAG)
        }
        synchronized(finishLock) { finishLock.wait() }
    }

    @JvmStatic
    fun loadProgress(stage: Int, progress: Int, max: Int) {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()
            ?: error("[DiskShaderCacheProgress] EmulationActivity not present")

        when (LoadCallbackStage.values()[stage]) {
            LoadCallbackStage.Prepare -> prepareDialog()
            LoadCallbackStage.Build -> fragment.onUpdateProgress(
                emulationActivity.getString(R.string.building_shaders),
                progress,
                max
            )
            LoadCallbackStage.Complete -> fragment.dismiss()
        }
    }

    // Equivalent to VideoCore::LoadCallbackStage
    enum class LoadCallbackStage {
        Prepare, Build, Complete
    }
}
