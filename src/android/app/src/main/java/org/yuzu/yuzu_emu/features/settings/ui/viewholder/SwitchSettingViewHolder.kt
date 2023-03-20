// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import android.widget.CompoundButton
import android.widget.TextView
import com.google.android.material.materialswitch.MaterialSwitch
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.view.SwitchSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class SwitchSettingViewHolder(itemView: View, adapter: SettingsAdapter) :
    SettingViewHolder(itemView, adapter) {
    private lateinit var item: SwitchSetting
    private lateinit var textSettingName: TextView
    private lateinit var textSettingDescription: TextView
    private lateinit var switch: MaterialSwitch

    override fun findViews(root: View) {
        textSettingName = root.findViewById(R.id.text_setting_name)
        textSettingDescription = root.findViewById(R.id.text_setting_description)
        switch = root.findViewById(R.id.switch_widget)
    }

    override fun bind(item: SettingsItem) {
        this.item = item as SwitchSetting
        textSettingName.setText(item.nameId)
        if (item.descriptionId!! > 0) {
            textSettingDescription.setText(item.descriptionId)
            textSettingDescription.visibility = View.VISIBLE
        } else {
            textSettingDescription.text = ""
            textSettingDescription.visibility = View.GONE
        }
        switch.isChecked = this.item.isChecked
        switch.setOnCheckedChangeListener { _: CompoundButton, _: Boolean ->
            adapter.onBooleanClick(item, bindingAdapterPosition, switch.isChecked)
        }
    }

    override fun onClick(clicked: View) {
        switch.toggle()
    }
}
