// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import android.widget.CheckBox
import android.widget.TextView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.view.CheckBoxSetting
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class CheckBoxSettingViewHolder(itemView: View, adapter: SettingsAdapter) :
    SettingViewHolder(itemView, adapter) {
    private lateinit var item: CheckBoxSetting
    private lateinit var textSettingName: TextView
    private lateinit var textSettingDescription: TextView
    private lateinit var checkbox: CheckBox

    override fun findViews(root: View) {
        textSettingName = root.findViewById(R.id.text_setting_name)
        textSettingDescription = root.findViewById(R.id.text_setting_description)
        checkbox = root.findViewById(R.id.checkbox)
    }

    override fun bind(item: SettingsItem) {
        this.item = item as CheckBoxSetting
        textSettingName.setText(item.nameId)
        if (item.descriptionId!! > 0) {
            textSettingDescription.setText(item.descriptionId)
            textSettingDescription.visibility = View.VISIBLE
        } else {
            textSettingDescription.text = ""
            textSettingDescription.visibility = View.GONE
        }
        checkbox.isChecked = this.item.isChecked
    }

    override fun onClick(clicked: View) {
        checkbox.toggle()
        adapter.onBooleanClick(item, bindingAdapterPosition, checkbox.isChecked)
    }
}
