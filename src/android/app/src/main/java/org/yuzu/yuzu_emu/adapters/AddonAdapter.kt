// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import org.yuzu.yuzu_emu.databinding.ListItemAddonBinding
import org.yuzu.yuzu_emu.model.Addon
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class AddonAdapter : AbstractDiffAdapter<Addon, AddonAdapter.AddonViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddonViewHolder {
        ListItemAddonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return AddonViewHolder(it) }
    }

    inner class AddonViewHolder(val binding: ListItemAddonBinding) :
        AbstractViewHolder<Addon>(binding) {
        override fun bind(model: Addon) {
            binding.root.setOnClickListener {
                binding.addonSwitch.isChecked = !binding.addonSwitch.isChecked
            }
            binding.title.text = model.title
            binding.version.text = model.version
            binding.addonSwitch.setOnCheckedChangeListener { _, checked ->
                model.enabled = checked
            }
            binding.addonSwitch.isChecked = model.enabled
        }
    }
}
