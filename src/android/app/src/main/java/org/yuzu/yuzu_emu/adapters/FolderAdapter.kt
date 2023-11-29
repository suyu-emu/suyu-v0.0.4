// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.net.Uri
import android.text.TextUtils
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.AsyncDifferConfig
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.CardFolderBinding
import org.yuzu.yuzu_emu.fragments.GameFolderPropertiesDialogFragment
import org.yuzu.yuzu_emu.model.GameDir
import org.yuzu.yuzu_emu.model.GamesViewModel

class FolderAdapter(val activity: FragmentActivity, val gamesViewModel: GamesViewModel) :
    ListAdapter<GameDir, FolderAdapter.FolderViewHolder>(
        AsyncDifferConfig.Builder(DiffCallback()).build()
    ) {
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): FolderAdapter.FolderViewHolder {
        CardFolderBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .also { return FolderViewHolder(it) }
    }

    override fun onBindViewHolder(holder: FolderAdapter.FolderViewHolder, position: Int) =
        holder.bind(currentList[position])

    inner class FolderViewHolder(val binding: CardFolderBinding) :
        RecyclerView.ViewHolder(binding.root) {
        private lateinit var gameDir: GameDir

        fun bind(gameDir: GameDir) {
            this.gameDir = gameDir

            binding.apply {
                path.text = Uri.parse(gameDir.uriString).path
                path.postDelayed(
                    {
                        path.isSelected = true
                        path.ellipsize = TextUtils.TruncateAt.MARQUEE
                    },
                    3000
                )

                buttonEdit.setOnClickListener {
                    GameFolderPropertiesDialogFragment.newInstance(this@FolderViewHolder.gameDir)
                        .show(
                            activity.supportFragmentManager,
                            GameFolderPropertiesDialogFragment.TAG
                        )
                }

                buttonDelete.setOnClickListener {
                    gamesViewModel.removeFolder(this@FolderViewHolder.gameDir)
                }
            }
        }
    }

    private class DiffCallback : DiffUtil.ItemCallback<GameDir>() {
        override fun areItemsTheSame(oldItem: GameDir, newItem: GameDir): Boolean {
            return oldItem == newItem
        }

        override fun areContentsTheSame(oldItem: GameDir, newItem: GameDir): Boolean {
            return oldItem == newItem
        }
    }
}
