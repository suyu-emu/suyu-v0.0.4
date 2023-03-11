// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import android.widget.TextView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

class HeaderViewHolder(itemView: View, adapter: SettingsAdapter) :
    SettingViewHolder(itemView, adapter) {
    private lateinit var headerName: TextView

    init {
        itemView.setOnClickListener(null)
    }

    override fun findViews(root: View) {
        headerName = root.findViewById(R.id.text_header_name)
    }

    override fun bind(item: SettingsItem) {
        headerName.setText(item.nameId)
    }

    override fun onClick(clicked: View) {
        // no-op
    }
}
