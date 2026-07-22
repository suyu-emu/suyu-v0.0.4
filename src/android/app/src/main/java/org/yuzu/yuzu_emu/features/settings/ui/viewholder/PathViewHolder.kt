// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import androidx.core.content.res.ResourcesCompat
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.PathSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter
import org.yuzu.yuzu_emu.utils.PathUtil
import org.yuzu.yuzu_emu.utils.ViewUtils.setVisible

class PathViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {

    private lateinit var setting: PathSetting

    override fun bind(item: SettingsItem) {
        setting = item as PathSetting
        binding.icon.setVisible(setting.iconId != 0)
        if (setting.iconId != 0) {
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.resources,
                    setting.iconId,
                    binding.icon.context.theme
                )
            )
        }

        binding.textSettingName.text = setting.title
        binding.textSettingDescription.setVisible(setting.description.isNotEmpty())
        binding.textSettingDescription.text = setting.description

        val currentPath = setting.getCurrentPath()
        val displayPath = PathUtil.truncatePathForDisplay(currentPath)

        binding.textSettingValue.setVisible(true)
        binding.textSettingValue.text = if (setting.isUsingDefaultPath()) {
            binding.root.context.getString(R.string.default_string)
        } else {
            displayPath
        }

        binding.buttonClear.setVisible(!setting.isUsingDefaultPath())
        binding.buttonClear.text = binding.root.context.getString(R.string.reset_to_default)
        binding.buttonClear.setOnClickListener {
            adapter.onPathReset(setting, bindingAdapterPosition)
        }

        setStyle(true, binding)
    }

    override fun onClick(clicked: View) {
        adapter.onPathClick(setting, bindingAdapterPosition)
    }

    override fun onLongClick(clicked: View): Boolean {
        return false
    }
}
