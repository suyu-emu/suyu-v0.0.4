// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.content.Intent
import android.graphics.Bitmap
import android.graphics.drawable.LayerDrawable
import android.net.Uri
import android.text.TextUtils
import android.view.LayoutInflater
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
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.HomeNavigationDirections
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.activities.EmulationActivity
import org.yuzu.yuzu_emu.databinding.CardGameBinding
import org.yuzu.yuzu_emu.model.Game
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.utils.GameIconUtils
import org.yuzu.yuzu_emu.viewholder.AbstractViewHolder

class GameAdapter(private val activity: AppCompatActivity) :
    AbstractDiffAdapter<Game, GameAdapter.GameViewHolder>() {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): GameViewHolder {
        CardGameBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return GameViewHolder(it) }
    }

    inner class GameViewHolder(val binding: CardGameBinding) :
        AbstractViewHolder<Game>(binding) {
        override fun bind(model: Game) {
            binding.imageGameScreen.scaleType = ImageView.ScaleType.CENTER_CROP
            GameIconUtils.loadGameIcon(model, binding.imageGameScreen)

            binding.textGameTitle.text = model.title.replace("[\\t\\n\\r]+".toRegex(), " ")

            binding.textGameTitle.postDelayed(
                {
                    binding.textGameTitle.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.textGameTitle.isSelected = true
                },
                3000
            )

            binding.cardGame.setOnClickListener { onClick(model) }
            binding.cardGame.setOnLongClickListener { onLongClick(model) }
        }

        fun onClick(game: Game) {
            val gameExists = DocumentFile.fromSingleUri(
                YuzuApplication.appContext,
                Uri.parse(game.path)
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

            val preferences =
                PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)
            preferences.edit()
                .putLong(
                    game.keyLastPlayedTime,
                    System.currentTimeMillis()
                )
                .apply()

            val openIntent =
                Intent(YuzuApplication.appContext, EmulationActivity::class.java).apply {
                    action = Intent.ACTION_VIEW
                    data = Uri.parse(game.path)
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
                        GameIconUtils.getGameIcon(activity, game)
                            .toDrawable(YuzuApplication.appContext.resources)
                    )
                    val inset = YuzuApplication.appContext.resources
                        .getDimensionPixelSize(R.dimen.icon_inset)
                    layerDrawable.setLayerInset(1, inset, inset, inset, inset)
                    val shortcut =
                        ShortcutInfoCompat.Builder(YuzuApplication.appContext, game.path)
                            .setShortLabel(game.title)
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

            val action = HomeNavigationDirections.actionGlobalEmulationActivity(game, true)
            binding.root.findNavController().navigate(action)
        }

        fun onLongClick(game: Game): Boolean {
            val action = HomeNavigationDirections.actionGlobalPerGamePropertiesFragment(game)
            binding.root.findNavController().navigate(action)
            return true
        }
    }
}
