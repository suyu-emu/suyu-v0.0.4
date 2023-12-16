// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.ListItemAddonBinding
import org.yuzu.yuzu_emu.model.Addon

class AddonAdapter : ListAdapter<Addon, AddonAdapter.AddonViewHolder>(
    AsyncDifferConfig.Builder(DiffCallback()).build()
) {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AddonViewHolder {
        ListItemAddonBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return AddonViewHolder(it) }
    }

    override fun getItemCount(): Int = currentList.size

    override fun onBindViewHolder(holder: AddonViewHolder, position: Int) =
        holder.bind(currentList[position])

    inner class AddonViewHolder(val binding: ListItemAddonBinding) :
        RecyclerView.ViewHolder(binding.root) {
        fun bind(addon: Addon) {
            binding.root.setOnClickListener {
                binding.addonSwitch.isChecked = !binding.addonSwitch.isChecked
            }
            binding.title.text = addon.title
            binding.version.text = addon.version
            binding.addonSwitch.setOnCheckedChangeListener { _, checked ->
                addon.enabled = checked
            }
            binding.addonSwitch.isChecked = addon.enabled
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<Addon>() {
        override fun areItemsTheSame(oldItem: Addon, newItem: Addon): Boolean {
            return oldItem == newItem
        }

        override fun areContentsTheSame(oldItem: Addon, newItem: Addon): Boolean {
            return oldItem == newItem
        }
    }
}
