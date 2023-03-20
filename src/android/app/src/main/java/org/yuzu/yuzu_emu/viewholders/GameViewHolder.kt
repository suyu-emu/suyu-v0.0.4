// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.viewholders

import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.CardGameBinding
import org.yuzu.yuzu_emu.model.Game

class GameViewHolder(val binding: CardGameBinding) : RecyclerView.ViewHolder(binding.root) {
    lateinit var game: Game

    init {
        itemView.tag = this
    }
}
