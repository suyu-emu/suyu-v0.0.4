package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import android.widget.TextView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.model.view.SubmenuSetting
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class SubmenuViewHolder(itemView: View, adapter: SettingsAdapter) :
    SettingViewHolder(itemView, adapter) {
    private lateinit var item: SubmenuSetting
    private lateinit var textSettingName: TextView
    private lateinit var textSettingDescription: TextView

    override fun findViews(root: View) {
        textSettingName = root.findViewById(R.id.text_setting_name)
        textSettingDescription = root.findViewById(R.id.text_setting_description)
    }

    override fun bind(item: SettingsItem) {
        this.item = item as SubmenuSetting
        textSettingName.setText(item.nameId)
        if (item.descriptionId!! > 0) {
            textSettingDescription.setText(item.descriptionId)
            textSettingDescription.visibility = View.VISIBLE
        } else {
            textSettingDescription.visibility = View.GONE
        }
    }

    override fun onClick(clicked: View) {
        adapter.onSubmenuClick(item)
    }
}
