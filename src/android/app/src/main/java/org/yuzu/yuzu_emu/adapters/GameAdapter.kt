// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.drawable.LayerDrawable
import android.net.Uri
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.ImageView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.pm.ShortcutInfoCompat
import androidx.core.content.pm.ShortcutManagerCompat
import androidx.core.content.res.ResourcesCompat
import androidx.core.graphics.drawable.IconCompat
import androidx.core.graphics.drawable.toBitmap
import androidx.core.graphics.drawable.toDrawable
import androidx.documentfile.provider.DocumentFile
import androidx.lifecycle.ViewModelProvider
import androidx.lifecycle.lifecycleScope
import androidx.navigation.findNavController
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.HomeNavigationDirections
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.adapters.GameAdapter.GameViewHolder
import org.yuzu.yuzu_emu.databinding.CardGameBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.utils.GameIconUtils

class GameAdapter(private val activity: AppCompatActivity) :
    ListAdapter<Game, GameViewHolder>(AsyncDifferConfig.Builder(DiffCallback()).build()),
    View.OnClickListener,
    View.OnLongClickListener {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        // Create a new view.
        val binding = CardGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        binding.cardGame.setOnClickListener(this)
        binding.cardGame.setOnLongClickListener(this)

        // Use that view to create a ViewHolder.
        return GameViewHolder(binding)
    }

    override fun onBindViewHolder(holder: GameViewHolder, position: Int) =
        holder.bind(currentList[position])

    override fun getItemCount(): Int = currentList.size

    /**
     * Launches the game that was clicked on.
     *
     * @param view The card representing the game the user wants to play.
     */
    override fun onClick(view: View) {
        val holder = view.tag as GameViewHolder

        val gameExists = DocumentFile.fromSingleUri(
            YuzuApplication.appContext,
            Uri.parse(holder.game.path)
        )?.exists() == true
        if (!gameExists) {
            Toast.makeText(
                YuzuApplication.appContext,
                R.string.loader_error_file_not_found,
                Toast.LENGTH_LONG
            ).show()

            ViewModelProvider(activity)[GamesViewModel::class.java].reloadGames(true)
            return
        }

        val preferences = PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
        preferences.edit()
            .putLong(
                holder.game.keyLastPlayedTime,
                System.currentTimeMillis()
            )
            .apply()

        val openIntent = Intent(YuzuApplication.appContext, EmulationActivity::class.java).apply {
            action = Intent.ACTION_VIEW
            data = Uri.parse(holder.game.path)
        }

        activity.lifecycleScope.launch {
            withContext(Dispatchers.IO) {
                val layerDrawable = ResourcesCompat.getDrawable(
                    YuzuApplication.appContext.resources,
                    R.drawable.shortcut,
                    null
                ) as LayerDrawable
                layerDrawable.setDrawableByLayerId(
                    R.id.shortcut_foreground,
                    GameIconUtils.getGameIcon(activity, holder.game)
                        .toDrawable(YuzuApplication.appContext.resources)
                )
                val inset = YuzuApplication.appContext.resources
                    .getDimensionPixelSize(R.dimen.icon_inset)
                layerDrawable.setLayerInset(1, inset, inset, inset, inset)
                val shortcut =
                    ShortcutInfoCompat.Builder(YuzuApplication.appContext, holder.game.path)
                        .setShortLabel(holder.game.title)
                        .setIcon(
                            IconCompat.createWithAdaptiveBitmap(
                                layerDrawable.toBitmap(config = Bitmap.Config.ARGB_8888)
                            )
                        )
                        .setIntent(openIntent)
                        .build()
                ShortcutManagerCompat.pushDynamicShortcut(YuzuApplication.appContext, shortcut)
            }
        }

        val action = HomeNavigationDirections.actionGlobalEmulationActivity(holder.game, true)
        view.findNavController().navigate(action)
    }

    override fun onLongClick(view: View): Boolean {
        val holder = view.tag as GameViewHolder
        val action = HomeNavigationDirections.actionGlobalPerGamePropertiesFragment(holder.game)
        view.findNavController().navigate(action)
        return true
    }

    inner class GameViewHolder(val binding: CardGameBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var game: Game

        init {
            binding.cardGame.tag = this
        }

        fun bind(game: Game) {
            this.game = game

            binding.imageGameScreen.scaleType = ImageView.ScaleType.CENTER_CROP
            GameIconUtils.loadGameIcon(game, binding.imageGameScreen)

            binding.textGameTitle.text = game.title.replace("[\\t\\n\\r]+".toRegex(), " ")

            binding.textGameTitle.postDelayed(
                {
                    binding.textGameTitle.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.textGameTitle.isSelected = true
                },
                3000
            )
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<Game>() {
        override fun areItemsTheSame(oldItem: Game, newItem: Game): Boolean {
            return oldItem == newItem
        }

        override fun areContentsTheSame(oldItem: Game, newItem: Game): Boolean {
            return oldItem == newItem
        }
    }
}
