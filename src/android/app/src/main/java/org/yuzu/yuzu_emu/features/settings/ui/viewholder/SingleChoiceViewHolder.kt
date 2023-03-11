// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import android.widget.TextView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.model.view.StringSingleChoiceSetting
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class SingleChoiceViewHolder(itemView: View, adapter: SettingsAdapter) :
    SettingViewHolder(itemView, adapter) {
    private lateinit var item: SettingsItem
    private lateinit var textSettingName: TextView
    private lateinit var textSettingDescription: TextView

    override fun findViews(root: View) {
        textSettingName = root.findViewById(R.id.text_setting_name)
        textSettingDescription = root.findViewById(R.id.text_setting_description)
    }

    override fun bind(item: SettingsItem) {
        this.item = item
        textSettingName.setText(item.nameId)
        textSettingDescription.visibility = View.VISIBLE
        if (item.descriptionId!! > 0) {
            textSettingDescription.setText(item.descriptionId)
        } else if (item is SingleChoiceSetting) {
            val resMgr = textSettingDescription.context.resources
            val values = resMgr.getIntArray(item.valuesId)
            for (i in values.indices) {
                if (values[i] == item.selectedValue) {
                    textSettingDescription.text = resMgr.getStringArray(item.choicesId)[i]
                }
            }
        } else {
            textSettingDescription.visibility = View.GONE
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
