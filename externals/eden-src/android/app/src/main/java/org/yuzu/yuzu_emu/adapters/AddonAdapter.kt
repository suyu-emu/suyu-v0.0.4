// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import org.yuzu.yuzu_emu.databinding.ListItemAddonBinding
import org.yuzu.yuzu_emu.model.Patch
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class AddonAdapter(val addonViewModel: AddonViewModel) :
    AbstractDiffAdapter<Patch, AddonAdapter.AddonViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddonViewHolder {
        ListItemAddonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return AddonViewHolder(it) }
    }

    inner class AddonViewHolder(val binding: ListItemAddonBinding) :
        AbstractViewHolder<Patch>(binding) {
        override fun bind(model: Patch) {
            binding.addonCard.setOnClickListener {
                binding.addonSwitch.performClick()
            }
            binding.title.text = model.name
            binding.version.text = model.version
            binding.addonSwitch.isChecked = model.enabled

            binding.addonSwitch.setOnCheckedChangeListener { _, checked ->
                model.enabled = checked
            }

            val deleteAction = {
                addonViewModel.setAddonToDelete(model)
            }
            binding.deleteCard.setOnClickListener { deleteAction() }
            binding.buttonDelete.setOnClickListener { deleteAction() }
        }
    }
}
