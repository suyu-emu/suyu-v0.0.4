// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.disk_shader_cache

import androidx.annotation.Keep
import androidx.lifecycle.ViewModelProvider
import org.suyu.suyu_emu.NativeLibrary
import org.suyu.suyu_emu.R
import org.suyu.suyu_emu.activities.EmulationActivity
import org.suyu.suyu_emu.model.EmulationViewModel
import org.suyu.suyu_emu.utils.Log

@Keep
object DiskShaderCacheProgress {
    private lateinit var emulationViewModel: EmulationViewModel

    private fun prepareViewModel() {
        emulationViewModel =
            ViewModelProvider(
                NativeLibrary.sEmulationActivity.get() as EmulationActivity
            )[EmulationViewModel::class.java]
    }

    @JvmStatic
    fun loadProgress(stage: Int, progress: Int, max: Int) {
        val emulationActivity = NativeLibrary.sEmulationActivity.get()
        if (emulationActivity == null) {
            Log.error("[DiskShaderCacheProgress] EmulationActivity not present")
            return
        }

        emulationActivity.runOnUiThread {
            when (LoadCallbackStage.values()[stage]) {
                LoadCallbackStage.Prepare -> prepareViewModel()
                LoadCallbackStage.Build -> emulationViewModel.updateProgress(
                    emulationActivity.getString(R.string.building_shaders),
                    progress,
                    max
                )

                LoadCallbackStage.Complete -> {}
            }
        }
    }

    // Equivalent to VideoCore::LoadCallbackStage
    enum class LoadCallbackStage {
        Prepare, Build, Complete
    }
}
