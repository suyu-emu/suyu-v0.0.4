// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.GpuUnswizzleSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible

class GpuUnswizzleViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: GpuUnswizzleSetting

    override fun bind(item: SettingsItem) {
        setting = item as GpuUnswizzleSetting
        binding.textSettingName.text = setting.title
        binding.textSettingDescription.setVisible(item.description.isNotEmpty())
        binding.textSettingDescription.text = item.description

        binding.textSettingValue.setVisible(true)
        val resMgr = binding.root.context.resources

        if (setting.isEnabled()) {
            // Show a summary of current settings
            val textureSizeEntries = resMgr.getStringArray(setting.textureSizeChoicesId)
            val textureSizeValues = resMgr.getIntArray(setting.textureSizeValuesId)
            val textureSizeIndex = textureSizeValues.indexOf(setting.getTextureSize())
            val textureSizeLabel = if (textureSizeIndex >= 0) textureSizeEntries[textureSizeIndex] else "?"

            val streamSizeEntries = resMgr.getStringArray(setting.streamSizeChoicesId)
            val streamSizeValues = resMgr.getIntArray(setting.streamSizeValuesId)
            val streamSizeIndex = streamSizeValues.indexOf(setting.getStreamSize())
            val streamSizeLabel = if (streamSizeIndex >= 0) streamSizeEntries[streamSizeIndex] else "?"

            val chunkSizeEntries = resMgr.getStringArray(setting.chunkSizeChoicesId)
            val chunkSizeValues = resMgr.getIntArray(setting.chunkSizeValuesId)
            val chunkSizeIndex = chunkSizeValues.indexOf(setting.getChunkSize())
            val chunkSizeLabel = if (chunkSizeIndex >= 0) chunkSizeEntries[chunkSizeIndex] else "?"

            binding.textSettingValue.text = "$textureSizeLabel ⋅ $streamSizeLabel ⋅ $chunkSizeLabel"
        } else {
            binding.textSettingValue.text = resMgr.getString(R.string.gpu_unswizzle_disabled)
        }

        binding.buttonClear.setVisible(setting.clearable)
        binding.buttonClear.setOnClickListener {
            adapter.onClearClick(setting, bindingAdapterPosition)
        }

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (!setting.isEditable) {
            return
        }

        adapter.onGpuUnswizzleClick(setting, bindingAdapterPosition)
    }

    override fun onLongClick(clicked: View): Boolean {
        if (setting.isEditable) {
            return adapter.onLongClick(setting, bindingAdapterPosition)
        }
        return false
    }
}
