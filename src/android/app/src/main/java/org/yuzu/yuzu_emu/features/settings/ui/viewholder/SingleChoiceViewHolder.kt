// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.model.view.StringSingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class SingleChoiceViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var item: SettingsItem

    override fun bind(item: SettingsItem) {
        this.item = item
        binding.textSettingName.setText(item.nameId)
        binding.textSettingDescription.visibility = View.VISIBLE
        if (item.descriptionId!! > 0) {
            binding.textSettingDescription.setText(item.descriptionId)
        } else if (item is SingleChoiceSetting) {
            val resMgr = binding.textSettingDescription.context.resources
            val values = resMgr.getIntArray(item.valuesId)
            for (i in values.indices) {
                if (values[i] == item.selectedValue) {
                    binding.textSettingDescription.text = resMgr.getStringArray(item.choicesId)[i]
                }
            }
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
    }

    override fun onClick(clicked: View) {
        if (item is SingleChoiceSetting) {
            adapter.onSingleChoiceClick(
                (item as SingleChoiceSetting),
                bindingAdapterPosition
            )
        } else if (item is StringSingleChoiceSetting) {
            adapter.onStringSingleChoiceClick(
                (item as StringSingleChoiceSetting),
                bindingAdapterPosition
            )
        }
    }
}
