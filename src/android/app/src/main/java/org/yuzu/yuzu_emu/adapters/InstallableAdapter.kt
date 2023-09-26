// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.CardInstallableBinding
import org.yuzu.yuzu_emu.model.Installable

class InstallableAdapter(private val installables: List<Installable>) :
    RecyclerView.Adapter<InstallableAdapter.InstallableViewHolder>() {
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): InstallableAdapter.InstallableViewHolder {
        val binding =
            CardInstallableBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        return InstallableViewHolder(binding)
    }

    override fun getItemCount(): Int = installables.size

    override fun onBindViewHolder(holder: InstallableAdapter.InstallableViewHolder, position: Int) =
        holder.bind(installables[position])

    inner class InstallableViewHolder(val binding: CardInstallableBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var installable: Installable

        fun bind(installable: Installable) {
            this.installable = installable

            binding.title.setText(installable.titleId)
            binding.description.setText(installable.descriptionId)

            if (installable.install != null) {
                binding.buttonInstall.visibility = View.VISIBLE
                binding.buttonInstall.setOnClickListener { installable.install.invoke() }
            }
            if (installable.export != null) {
                binding.buttonExport.visibility = View.VISIBLE
                binding.buttonExport.setOnClickListener { installable.export.invoke() }
            }
        }
    }
}
