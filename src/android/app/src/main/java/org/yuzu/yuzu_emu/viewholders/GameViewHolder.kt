// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.viewholders

import android.view.View
import android.widget.ImageView
import android.widget.TextView
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.R

/**
 * A simple class that stores references to views so that the GameAdapter doesn't need to
 * keep calling findViewById(), which is expensive.
 */
class GameViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
    var imageIcon: ImageView
    var textGameTitle: TextView
    var textGameCaption: TextView
    var gameId: String? = null

    // TODO Not need any of this stuff. Currently only the properties dialog needs it.
    var path: String? = null
    var title: String? = null
    var description: String? = null
    var regions: String? = null
    var company: String? = null

    init {
        itemView.tag = this
        imageIcon = itemView.findViewById(R.id.image_game_screen)
        textGameTitle = itemView.findViewById(R.id.text_game_title)
        textGameCaption = itemView.findViewById(R.id.text_game_caption)
    }
}
