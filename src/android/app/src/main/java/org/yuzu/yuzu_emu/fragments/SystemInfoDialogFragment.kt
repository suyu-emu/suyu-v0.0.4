// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.ActivityManager
import android.app.Dialog
import android.content.Context
import android.os.Build
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogSystemInfoBinding

class SystemInfoDialogFragment : DialogFragment() {
    private var _binding: DialogSystemInfoBinding? = null
    private val binding get() = _binding!!

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        _binding = DialogSystemInfoBinding.inflate(layoutInflater)

        populateSystemInfo()

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.system_information)
            .setPositiveButton(android.R.string.ok, null)
            .create()

        dialog.setView(binding.root)

        return dialog
    }

    private fun populateSystemInfo() {
        val systemInfo = buildString {
            // General Device Info
            appendLine("=== ${getString(R.string.general_information)} ===")
            appendLine("${getString(R.string.device_manufacturer)}: ${Build.MANUFACTURER}")
            appendLine("${getString(R.string.device_model)}: ${Build.MODEL}")
            appendLine("${getString(R.string.device_name)}: ${Build.DEVICE}")
            appendLine("${getString(R.string.product)}: ${Build.PRODUCT}")
            appendLine("${getString(R.string.hardware)}: ${Build.HARDWARE}")
            appendLine("${getString(R.string.supported_abis)}: ${Build.SUPPORTED_ABIS.joinToString(", ")}")
            appendLine("${getString(R.string.android_version)}: ${Build.VERSION.RELEASE} (API ${Build.VERSION.SDK_INT})")
            appendLine("${getString(R.string.android_security_patch)}: ${Build.VERSION.SECURITY_PATCH}")
            appendLine("${getString(R.string.build_id)}: ${Build.ID}")

            appendLine()
            appendLine("=== ${getString(R.string.cpu_info)} ===")

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S && Build.SOC_MODEL.isNotBlank()) {
                appendLine("${getString(R.string.soc)} ${Build.SOC_MODEL}")
            }

            val cpuSummary = NativeLibrary.getCpuSummary()
            if (cpuSummary.isNotEmpty() && cpuSummary != "Unknown") {
                appendLine(cpuSummary)
            }

            appendLine()

            // GPU Info
            appendLine("=== ${getString(R.string.gpu_information)} ===")
            try {
                val gpuModel = NativeLibrary.getGpuModel()
                appendLine("${getString(R.string.gpu_model)}: $gpuModel")

                val vulkanApi = NativeLibrary.getVulkanApiVersion()
                appendLine("Vulkan API: $vulkanApi")

                val vulkanDriver = NativeLibrary.getVulkanDriverVersion()
                appendLine("${getString(R.string.vulkan_driver_version)}: $vulkanDriver")
            } catch (e: Exception) {
                appendLine("${getString(R.string.error_getting_emulator_info)}: ${e.message}")
            }
            appendLine()

            // Memory Info
            appendLine("=== ${getString(R.string.memory_info)} ===")

            val activityManager =
                requireContext().getSystemService(Context.ACTIVITY_SERVICE) as ActivityManager
            val memInfo = ActivityManager.MemoryInfo()
            activityManager.getMemoryInfo(memInfo)
            val totalDeviceRam = memInfo.totalMem / (1024 * 1024)

            appendLine("${getString(R.string.total_memory)}: $totalDeviceRam MB")
        }

        binding.textSystemInfo.text = systemInfo
    }


    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    companion object {
        const val TAG = "SystemInfoDialogFragment"

        fun newInstance(): SystemInfoDialogFragment {
            return SystemInfoDialogFragment()
        }
    }
}
