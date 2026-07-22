// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.graphics.Bitmap
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.ItemFirmwareAvatarBinding

class FirmwareAvatarAdapter(
    private val avatars: List<Bitmap>,
    private val onAvatarSelected: (Bitmap) -> Unit
) : RecyclerView.Adapter<FirmwareAvatarAdapter.AvatarViewHolder>() {

    private var selectedPosition = -1

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): AvatarViewHolder {
        val binding = ItemFirmwareAvatarBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return AvatarViewHolder(binding)
    }

    override fun onBindViewHolder(holder: AvatarViewHolder, position: Int) {
        holder.bind(avatars[position], position == selectedPosition)
    }

    override fun getItemCount(): Int = avatars.size

    inner class AvatarViewHolder(
        private val binding: ItemFirmwareAvatarBinding
    ) : RecyclerView.ViewHolder(binding.root) {

        fun bind(avatar: Bitmap, isSelected: Boolean) {
            binding.imageAvatar.setImageBitmap(avatar)
            binding.root.isChecked = isSelected

            binding.root.setOnClickListener {
                val previousSelected = selectedPosition
                selectedPosition = bindingAdapterPosition

                if (previousSelected != -1) {
                    notifyItemChanged(previousSelected)
                }
                notifyItemChanged(selectedPosition)

                onAvatarSelected(avatar)
            }
        }
    }
}
