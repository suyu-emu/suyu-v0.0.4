// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import org.suyu.suyu_emu.R
import org.suyu.suyu_emu.databinding.CardDriverOptionBinding
import org.suyu.suyu_emu.features.settings.model.StringSetting
import org.suyu.suyu_emu.model.Driver
import org.suyu.suyu_emu.model.DriverViewModel
import org.suyu.suyu_emu.utils.ViewUtils.marquee
import org.suyu.suyu_emu.utils.ViewUtils.setVisible
import org.suyu.suyu_emu.viewholder.AbstractViewHolder

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
                title.marquee()
                version.marquee()
                description.marquee()
                title.text = model.title
                version.text = model.version
                description.text = model.description
                buttonDelete.setVisible(
                    model.title != binding.root.context.getString(R.string.system_gpu_driver)
                )
            }
        }
    }
}
