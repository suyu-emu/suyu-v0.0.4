// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.LayoutInflater
import android.widget.ArrayAdapter
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogGpuUnswizzleBinding
import org.yuzu.yuzu_emu.features.settings.model.view.GpuUnswizzleSetting

class GpuUnswizzleDialogFragment : DialogFragment() {
    private var position = 0
    private val settingsViewModel: SettingsViewModel by activityViewModels()
    private lateinit var binding: DialogGpuUnswizzleBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        position = requireArguments().getInt(POSITION)

        if (settingsViewModel.clickedItem == null) dismiss()
    }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        binding = DialogGpuUnswizzleBinding.inflate(LayoutInflater.from(requireContext()))
        val item = settingsViewModel.clickedItem as GpuUnswizzleSetting

        // Setup texture size dropdown
        val textureSizeEntries = resources.getStringArray(item.textureSizeChoicesId)
        val textureSizeValues = resources.getIntArray(item.textureSizeValuesId)
        val textureSizeAdapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_dropdown_item_1line,
            textureSizeEntries.toMutableList()
        )
        binding.dropdownTextureSize.setAdapter(textureSizeAdapter)

        // Setup stream size dropdown
        val streamSizeEntries = resources.getStringArray(item.streamSizeChoicesId)
        val streamSizeValues = resources.getIntArray(item.streamSizeValuesId)
        val streamSizeAdapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_dropdown_item_1line,
            streamSizeEntries.toMutableList()
        )
        binding.dropdownStreamSize.setAdapter(streamSizeAdapter)

        // Setup chunk size dropdown
        val chunkSizeEntries = resources.getStringArray(item.chunkSizeChoicesId)
        val chunkSizeValues = resources.getIntArray(item.chunkSizeValuesId)
        val chunkSizeAdapter = ArrayAdapter(
            requireContext(),
            android.R.layout.simple_dropdown_item_1line,
            chunkSizeEntries.toMutableList()
        )
        binding.dropdownChunkSize.setAdapter(chunkSizeAdapter)

        // Load current values
        val isEnabled = item.isEnabled()
        binding.switchEnable.isChecked = isEnabled

        if (isEnabled) {
            val textureSizeIndex = textureSizeValues.indexOf(item.getTextureSize())
            if (textureSizeIndex >= 0) {
                binding.dropdownTextureSize.setText(textureSizeEntries[textureSizeIndex], false)
            }

            val streamSizeIndex = streamSizeValues.indexOf(item.getStreamSize())
            if (streamSizeIndex >= 0) {
                binding.dropdownStreamSize.setText(streamSizeEntries[streamSizeIndex], false)
            }

            val chunkSizeIndex = chunkSizeValues.indexOf(item.getChunkSize())
            if (chunkSizeIndex >= 0) {
                binding.dropdownChunkSize.setText(chunkSizeEntries[chunkSizeIndex], false)
            }
        } else {
            // Set default/recommended values when disabling
            binding.dropdownTextureSize.setText(textureSizeEntries[3], false)
            binding.dropdownStreamSize.setText(streamSizeEntries[3], false)
            binding.dropdownChunkSize.setText(chunkSizeEntries[3], false)
        }

        // Clear adapter filters after setText to fix rotation bug
        textureSizeAdapter.filter.filter(null)
        streamSizeAdapter.filter.filter(null)
        chunkSizeAdapter.filter.filter(null)

        // Enable/disable dropdowns based on switch state
        updateDropdownsState(isEnabled)
        binding.switchEnable.setOnCheckedChangeListener { _, checked ->
            updateDropdownsState(checked)
        }

        val dialog = MaterialAlertDialogBuilder(requireContext())
            .setTitle(item.title)
            .setView(binding.root)
            .create()

        // Setup button listeners
        binding.btnDefault.setOnClickListener {
            // Reset to defaults
            item.reset()
            // Refresh values with adapters reset
            val textureSizeIndex = textureSizeValues.indexOf(item.getTextureSize())
            if (textureSizeIndex >= 0) {
                binding.dropdownTextureSize.setText(textureSizeEntries[textureSizeIndex], false)
            }
            val streamSizeIndex = streamSizeValues.indexOf(item.getStreamSize())
            if (streamSizeIndex >= 0) {
                binding.dropdownStreamSize.setText(streamSizeEntries[streamSizeIndex], false)
            }
            val chunkSizeIndex = chunkSizeValues.indexOf(item.getChunkSize())
            if (chunkSizeIndex >= 0) {
                binding.dropdownChunkSize.setText(chunkSizeEntries[chunkSizeIndex], false)
            }
            // Clear filters
            textureSizeAdapter.filter.filter(null)
            streamSizeAdapter.filter.filter(null)
            chunkSizeAdapter.filter.filter(null)

            settingsViewModel.setAdapterItemChanged(position)
            settingsViewModel.setShouldReloadSettingsList(true)
        }

        binding.btnCancel.setOnClickListener {
            dialog.dismiss()
        }

        binding.btnOk.setOnClickListener {
            if (binding.switchEnable.isChecked) {
                item.enable()
                // Save the selected values
                val selectedTextureIndex = textureSizeEntries.indexOf(
                    binding.dropdownTextureSize.text.toString()
                )
                if (selectedTextureIndex >= 0) {
                    item.setTextureSize(textureSizeValues[selectedTextureIndex])
                }

                val selectedStreamIndex = streamSizeEntries.indexOf(
                    binding.dropdownStreamSize.text.toString()
                )
                if (selectedStreamIndex >= 0) {
                    item.setStreamSize(streamSizeValues[selectedStreamIndex])
                }

                val selectedChunkIndex = chunkSizeEntries.indexOf(
                    binding.dropdownChunkSize.text.toString()
                )
                if (selectedChunkIndex >= 0) {
                    item.setChunkSize(chunkSizeValues[selectedChunkIndex])
                }
            } else {
                // Disable GPU unswizzle
                item.disable()
            }

            settingsViewModel.setAdapterItemChanged(position)
            settingsViewModel.setShouldReloadSettingsList(true)
            dialog.dismiss()
        }

        // Ensure filters are cleared after dialog is shown
        binding.root.post {
            textureSizeAdapter.filter.filter(null)
            streamSizeAdapter.filter.filter(null)
            chunkSizeAdapter.filter.filter(null)
        }

        return dialog
    }

    private fun updateDropdownsState(enabled: Boolean) {
        binding.layoutTextureSize.isEnabled = enabled
        binding.dropdownTextureSize.isEnabled = enabled
        binding.layoutStreamSize.isEnabled = enabled
        binding.dropdownStreamSize.isEnabled = enabled
        binding.layoutChunkSize.isEnabled = enabled
        binding.dropdownChunkSize.isEnabled = enabled
    }

    companion object {
        const val TAG = "GpuUnswizzleDialogFragment"
        const val POSITION = "Position"

        fun newInstance(
            settingsViewModel: SettingsViewModel,
            item: GpuUnswizzleSetting,
            position: Int
        ): GpuUnswizzleDialogFragment {
            val dialog = GpuUnswizzleDialogFragment()
            val args = Bundle()
            args.putInt(POSITION, position)
            dialog.arguments = args
            settingsViewModel.clickedItem = item
            return dialog
        }
    }
}
