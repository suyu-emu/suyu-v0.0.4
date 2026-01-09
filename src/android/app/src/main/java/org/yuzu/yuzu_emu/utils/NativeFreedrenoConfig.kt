// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.utils

/**
 * Provides access to Freedreno/Turnip driver configuration through JNI bindings.
 *
 * This class allows Java/Kotlin code to configure Freedreno environment variables
 * for the GPU driver (Turnip/Freedreno) that runs in the emulator on Android.
 *
 * Variables must be set BEFORE starting emulation for them to take effect.
 *
 * See https://docs.mesa3d.org/drivers/freedreno.html for documentation.
 */
object NativeFreedrenoConfig {

    @Synchronized
    external fun setFreedrenoBasePath(basePath: String)

    @Synchronized
    external fun initializeFreedrenoConfig()

    @Synchronized
    external fun saveFreedrenoConfig()

    @Synchronized
    external fun reloadFreedrenoConfig()

    @Synchronized
    external fun setFreedrenoEnv(varName: String, value: String): Boolean

    @Synchronized
    external fun getFreedrenoEnv(varName: String): String

    @Synchronized
    external fun isFreedrenoEnvSet(varName: String): Boolean

    @Synchronized
    external fun clearFreedrenoEnv(varName: String): Boolean

    @Synchronized
    external fun clearAllFreedrenoEnv()

    @Synchronized
    external fun getFreedrenoEnvSummary(): String

    @Synchronized
    external fun setCurrentProgramId(programId: String)

    @Synchronized
    external fun loadPerGameConfig(programId: String): Boolean

    @Synchronized
    external fun loadPerGameConfigWithGlobalFallback(programId: String): Boolean

    @Synchronized
    external fun savePerGameConfig(programId: String): Boolean

    @Synchronized
    external fun hasPerGameConfig(programId: String): Boolean

    @Synchronized
    external fun deletePerGameConfig(programId: String): Boolean
}

/**
 * Data class representing a Freedreno preset configuration.
 * Presets are commonly used debugging/profiling configurations.
 */
data class FreedrenoPreset(
    val name: String,           // Display name (e.g., "Debug - CPU Memory")
    val description: String,    // Description of what this preset does
    val icon: String,           // Icon identifier
    val variables: Map<String, String> // Map of env vars to set
)

/**
 * Predefined Freedreno presets for quick configuration.
 */
object FreedrenoPresets {

    val DEBUG_CPU_MEMORY = FreedrenoPreset(
        name = "Debug - CPU Memory",
        description = "Use CPU memory (slower but more stable)",
        icon = "ic_debug_cpu",
        variables = mapOf(
            "TU_DEBUG" to "sysmem"
        )
    )

    val DEBUG_UBWC_DISABLED = FreedrenoPreset(
        name = "Debug - No UBWC",
        description = "Disable UBWC compression for debugging",
        icon = "ic_debug_ubwc",
        variables = mapOf(
            "TU_DEBUG" to "noubwc"
        )
    )

    val DEBUG_NO_BINNING = FreedrenoPreset(
        name = "Debug - No Binning",
        description = "Disable binning optimization",
        icon = "ic_debug_bin",
        variables = mapOf(
            "TU_DEBUG" to "nobin"
        )
    )

    val CAPTURE_RENDERPASS = FreedrenoPreset(
        name = "Capture - Renderpass",
        description = "Capture command stream data for debugging",
        icon = "ic_capture",
        variables = mapOf(
            "FD_RD_DUMP" to "enable"
        )
    )

    val CAPTURE_FRAMES = FreedrenoPreset(
        name = "Capture - First 100 Frames",
        description = "Capture command stream for first 100 frames only",
        icon = "ic_capture",
        variables = mapOf(
            "FD_RD_DUMP" to "enable",
            "FD_RD_DUMP_FRAMES" to "0-100"
        )
    )

    val SHADER_DEBUG = FreedrenoPreset(
        name = "Shader Debug",
        description = "Enable IR3 shader compiler debugging",
        icon = "ic_shader",
        variables = mapOf(
            "IR3_SHADER_DEBUG" to "nouboopt,spillall"
        )
    )

    val GPU_HANG_TRACE = FreedrenoPreset(
        name = "GPU Hang Trace",
        description = "Trace GPU progress for debugging hangs",
        icon = "ic_hang_trace",
        variables = mapOf(
            "TU_BREADCRUMBS" to "1"
        )
    )

    val PERFORMANCE_DEFAULT = FreedrenoPreset(
        name = "Performance - Default",
        description = "Clear all debug options for performance",
        icon = "ic_performance",
        variables = emptyMap()  // Clears all when applied
    )

    val ALL_PRESETS = listOf(
        DEBUG_CPU_MEMORY,
        DEBUG_UBWC_DISABLED,
        DEBUG_NO_BINNING,
        CAPTURE_RENDERPASS,
        CAPTURE_FRAMES,
        SHADER_DEBUG,
        GPU_HANG_TRACE,
        PERFORMANCE_DEFAULT
    )
}
