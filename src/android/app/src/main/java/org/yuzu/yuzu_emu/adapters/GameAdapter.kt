// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.graphics.Bitmap
import android.graphics.BitmapFactory
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import androidx.appcompat.app.AppCompatActivity
import androidx.lifecycle.lifecycleScope
import androidx.recyclerview.widget.RecyclerView
import coil.load
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.CardGameBinding
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.model.Game
import kotlin.collections.ArrayList

/**
 * This adapter gets its information from a database Cursor. This fact, paired with the usage of
 * ContentProviders and Loaders, allows for efficient display of a limited view into a (possibly)
 * large dataset.
 */
class GameAdapter(private val activity: AppCompatActivity, var games: ArrayList<Game>) :
    RecyclerView.Adapter<GameAdapter.GameViewHolder>(),
    View.OnClickListener {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        // Create a new view.
        val binding = CardGameBinding.inflate(LayoutInflater.from(parent.context))
        binding.root.setOnClickListener(this)

        // Use that view to create a ViewHolder.
        return GameViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GameViewHolder, position: Int) {
        holder.bind(games[position])
    }

    override fun getItemCount(): Int {
        return games.size
    }

    /**
     * Launches the game that was clicked on.
     *
     * @param view The card representing the game the user wants to play.
     */
    override fun onClick(view: View) {
        val holder = view.tag as GameViewHolder
        EmulationActivity.launch((view.context as AppCompatActivity), holder.game)
    }

    inner class GameViewHolder(val binding: CardGameBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var game: Game

        init {
            itemView.tag = this
        }

        fun bind(game: Game) {
            this.game = game

            binding.imageGameScreen.scaleType = ImageView.ScaleType.CENTER_CROP
            activity.lifecycleScope.launch {
                val bitmap = decodeGameIcon(game.path)
                binding.imageGameScreen.load(bitmap) {
                    error(R.drawable.no_icon)
                    crossfade(true)
                }
            }

            binding.textGameTitle.text = game.title.replace("[\\t\\n\\r]+".toRegex(), " ")
            binding.textGameCaption.text = game.company

            if (game.company.isEmpty()) {
                binding.textGameCaption.visibility = View.GONE
            }
        }
    }

    fun swapData(games: ArrayList<Game>) {
        this.games = games
        notifyDataSetChanged()
    }

    private fun decodeGameIcon(uri: String): Bitmap? {
        val data = NativeLibrary.getIcon(uri)
        return BitmapFactory.decodeByteArray(
            data,
            0,
            data.size,
            BitmapFactory.Options()
        )
    }
}
