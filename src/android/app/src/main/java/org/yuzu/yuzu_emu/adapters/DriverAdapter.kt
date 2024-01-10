// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import org.yuzu.yuzu_emu.databinding.CardDriverOptionBinding
import org.yuzu.yuzu_emu.features.settings.model.StringSetting
import org.yuzu.yuzu_emu.model.Driver
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class DriverAdapter(private val driverViewModel: DriverViewModel) :
    AbstractSingleSelectionList<Driver, DriverAdapter.DriverViewHolder>(
        driverViewModel.driverList.value
    ) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DriverViewHolder {
        CardDriverOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return DriverViewHolder(it) }
    }

    inner class DriverViewHolder(val binding: CardDriverOptionBinding) :
        AbstractViewHolder<Driver>(binding) {
        override fun bind(model: Driver) {
            binding.apply {
                radioButton.isChecked = model.selected
                root.setOnClickListener {
                    selectItem(bindingAdapterPosition) {
                        driverViewModel.onDriverSelected(it)
                        driverViewModel.showClearButton(!StringSetting.DRIVER_PATH.global)
                    }
                }
                buttonDelete.setOnClickListener {
                    removeSelectableItem(
                        bindingAdapterPosition
                    ) { removedPosition: Int, selectedPosition: Int ->
                        driverViewModel.onDriverRemoved(removedPosition, selectedPosition)
                        driverViewModel.showClearButton(!StringSetting.DRIVER_PATH.global)
                    }
                }

                // Delay marquee by 3s
                title.postDelayed(
                    {
                        title.isSelected = true
                        title.ellipsize = TextUtils.TruncateAt.MARQUEE
                        version.isSelected = true
                        version.ellipsize = TextUtils.TruncateAt.MARQUEE
                        description.isSelected = true
                        description.ellipsize = TextUtils.TruncateAt.MARQUEE
                    },
                    3000
                )
                title.text = model.title
                version.text = model.version
                description.text = model.description
                if (model.description.isNotEmpty()) {
                    version.visibility = View.VISIBLE
                    description.visibility = View.VISIBLE
                    buttonDelete.visibility = View.VISIBLE
                } else {
                    version.visibility = View.GONE
                    description.visibility = View.GONE
                    buttonDelete.visibility = View.GONE
                }
            }
        }
    }
}
