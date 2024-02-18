// SPDX-FileCopyrightText: 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import org.yuzu.yuzu_emu.databinding.ListItemSettingInputBinding
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.features.settings.model.view.AnalogInputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.ButtonInputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.InputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.ModifierInputSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class InputViewHolder(val binding: ListItemSettingInputBinding, adapter: SettingsAdapter) :
    SettingViewHolder(binding.root, adapter) {
    private lateinit var setting: InputSetting

    override fun bind(item: SettingsItem) {
        setting = item as InputSetting
        binding.textSettingName.text = setting.title
        binding.textSettingValue.text = setting.getSelectedValue()

        binding.buttonOptions.visibility = when (item) {
            is AnalogInputSetting -> {
                val param = NativeInput.getStickParam(item.playerIndex, item.nativeAnalog)
                if (
                    param.get("engine", "") == "analog_from_button" ||
                    param.has("axis_x") || param.has("axis_y")
                ) {
                    View.VISIBLE
                } else {
                    View.GONE
                }
            }

            is ButtonInputSetting -> {
                val param = NativeInput.getButtonParam(item.playerIndex, item.nativeButton)
                if (
                    param.has("code") || param.has("button") || param.has("hat") ||
                    param.has("axis")
                ) {
                    View.VISIBLE
                } else {
                    View.GONE
                }
            }

            is ModifierInputSetting -> {
                val params = NativeInput.getStickParam(item.playerIndex, item.nativeAnalog)
                if (params.has("modifier")) {
                    View.VISIBLE
                } else {
                    View.GONE
                }
            }
        }

        binding.buttonOptions.setOnClickListener(null)
        binding.buttonOptions.setOnClickListener {
            adapter.onInputOptionsClick(binding.buttonOptions, setting, bindingAdapterPosition)
        }
    }

    override fun onClick(clicked: View) =
        adapter.onInputClick(setting, bindingAdapterPosition)

    override fun onLongClick(clicked: View): Boolean =
        adapter.onLongClick(setting, bindingAdapterPosition)
}
