// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.CardDriverOptionBinding
import org.yuzu.yuzu_emu.model.DriverViewModel
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import org.yuzu.yuzu_emu.utils.GpuDriverMetadata

class DriverAdapter(private val driverViewModel: DriverViewModel) :
    ListAdapter<Pair<String, GpuDriverMetadata>, DriverAdapter.DriverViewHolder>(
        AsyncDifferConfig.Builder(DiffCallback()).build()
    ) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DriverViewHolder {
        val binding =
            CardDriverOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return DriverViewHolder(binding)
    }

    override fun getItemCount(): Int = currentList.size

    override fun onBindViewHolder(holder: DriverViewHolder, position: Int) =
        holder.bind(currentList[position])

    private fun onSelectDriver(position: Int) {
        driverViewModel.setSelectedDriverIndex(position)
        notifyItemChanged(driverViewModel.previouslySelectedDriver)
        notifyItemChanged(driverViewModel.selectedDriver)
    }

    private fun onDeleteDriver(driverData: Pair<String, GpuDriverMetadata>, position: Int) {
        if (driverViewModel.selectedDriver > position) {
            driverViewModel.setSelectedDriverIndex(driverViewModel.selectedDriver - 1)
        }
        if (GpuDriverHelper.customDriverSettingData == driverData.second) {
            driverViewModel.setSelectedDriverIndex(0)
        }
        driverViewModel.driversToDelete.add(driverData.first)
        driverViewModel.removeDriver(driverData)
        notifyItemRemoved(position)
        notifyItemChanged(driverViewModel.selectedDriver)
    }

    inner class DriverViewHolder(val binding: CardDriverOptionBinding) :
        RecyclerView.ViewHolder(binding.root) {
        private lateinit var driverData: Pair<String, GpuDriverMetadata>

        fun bind(driverData: Pair<String, GpuDriverMetadata>) {
            this.driverData = driverData
            val driver = driverData.second

            binding.apply {
                radioButton.isChecked = driverViewModel.selectedDriver == bindingAdapterPosition
                root.setOnClickListener {
                    onSelectDriver(bindingAdapterPosition)
                }
                buttonDelete.setOnClickListener {
                    onDeleteDriver(driverData, bindingAdapterPosition)
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
                if (driver.name == null) {
                    title.setText(R.string.system_gpu_driver)
                    description.text = ""
                    version.text = ""
                    version.visibility = View.GONE
                    description.visibility = View.GONE
                    buttonDelete.visibility = View.GONE
                } else {
                    title.text = driver.name
                    version.text = driver.version
                    description.text = driver.description
                    version.visibility = View.VISIBLE
                    description.visibility = View.VISIBLE
                    buttonDelete.visibility = View.VISIBLE
                }
            }
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<Pair<String, GpuDriverMetadata>>() {
        override fun areItemsTheSame(
            oldItem: Pair<String, GpuDriverMetadata>,
            newItem: Pair<String, GpuDriverMetadata>
        ): Boolean {
            return oldItem.first == newItem.first
        }

        override fun areContentsTheSame(
            oldItem: Pair<String, GpuDriverMetadata>,
            newItem: Pair<String, GpuDriverMetadata>
        ): Boolean {
            return oldItem.second == newItem.second
        }
    }
}
