// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import androidx.core.content.res.ResourcesCompat
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SubmenuSetting
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class SubmenuViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: SubmenuSetting

    override fun bind(item: SettingsItem) {
        setting = item as SubmenuSetting
        if (setting.iconId != 0) {
            binding.icon.visibility = View.VISIBLE
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.resources,
                    setting.iconId,
                    binding.icon.context.theme
                )
            )
        } else {
            binding.icon.visibility = View.GONE
        }

        binding.textSettingName.text = setting.title
        if (setting.description.isNotEmpty()) {
            binding.textSettingDescription.text = setting.description
            binding.textSettingDescription.visibility = View.VISIBLE
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
        binding.textSettingValue.visibility = View.GONE
        binding.buttonClear.visibility = View.GONE
    }

    override fun onClick(clicked: View) {
        adapter.onSubmenuClick(setting)
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}
