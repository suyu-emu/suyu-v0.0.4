// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.databinding.ListItemSettingBinding
import org.yuzu.yuzu_emu.features.settings.model.view.RunnableSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class RunnableViewHolder(val binding: ListItemSettingBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: RunnableSetting

    override fun bind(item: SettingsItem) {
        setting = item as RunnableSetting
        binding.textSettingName.setText(item.nameId)
        if (item.descriptionId != 0) {
            binding.textSettingDescription.setText(item.descriptionId)
            binding.textSettingDescription.visibility = View.VISIBLE
        } else {
            binding.textSettingDescription.visibility = View.GONE
        }
        binding.textSettingValue.visibility = View.GONE

        setStyle(setting.isEditable, binding)
    }

    override fun onClick(clicked: View) {
        if (!setting.isRuntimeRunnable && !NativeLibrary.isRunning()) {
            setting.runnable.invoke()
        }
    }

    override fun onLongClick(clicked: View): Boolean {
        // no-op
        return true
    }
}
