// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.res.ResourcesCompat
import androidx.lifecycle.Lifecycle
import androidx.lifecycle.LifecycleOwner
import androidx.lifecycle.lifecycleScope
import androidx.lifecycle.repeatOnLifecycle
import androidx.recyclerview.widget.RecyclerView
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.databinding.CardInstallableIconBinding
import org.yuzu.yuzu_emu.databinding.CardSimpleOutlinedBinding
import org.yuzu.yuzu_emu.model.GameProperty
import org.yuzu.yuzu_emu.model.InstallableProperty
import org.yuzu.yuzu_emu.model.SubmenuProperty

class GamePropertiesAdapter(
    private val viewLifecycle: LifecycleOwner,
    private var properties: List<GameProperty>
) :
    RecyclerView.Adapter<GamePropertiesAdapter.GamePropertyViewHolder>() {
    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): GamePropertyViewHolder {
        val inflater = LayoutInflater.from(parent.context)
        return when (viewType) {
            PropertyType.Submenu.ordinal -> {
                SubmenuPropertyViewHolder(
                    CardSimpleOutlinedBinding.inflate(
                        inflater,
                        parent,
                        false
                    )
                )
            }

            else -> InstallablePropertyViewHolder(
                CardInstallableIconBinding.inflate(
                    inflater,
                    parent,
                    false
                )
            )
        }
    }

    override fun getItemCount(): Int = properties.size

    override fun onBindViewHolder(holder: GamePropertyViewHolder, position: Int) =
        holder.bind(properties[position])

    override fun getItemViewType(position: Int): Int {
        return when (properties[position]) {
            is SubmenuProperty -> PropertyType.Submenu.ordinal
            else -> PropertyType.Installable.ordinal
        }
    }

    sealed class GamePropertyViewHolder(itemView: View) : RecyclerView.ViewHolder(itemView) {
        abstract fun bind(property: GameProperty)
    }

    inner class SubmenuPropertyViewHolder(val binding: CardSimpleOutlinedBinding) :
        GamePropertyViewHolder(binding.root) {
        override fun bind(property: GameProperty) {
            val submenuProperty = property as SubmenuProperty

            binding.root.setOnClickListener {
                submenuProperty.action.invoke()
            }

            binding.title.setText(submenuProperty.titleId)
            binding.description.setText(submenuProperty.descriptionId)
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.context.resources,
                    submenuProperty.iconId,
                    binding.icon.context.theme
                )
            )

            binding.details.postDelayed({
                binding.details.isSelected = true
                binding.details.ellipsize = TextUtils.TruncateAt.MARQUEE
            }, 3000)

            if (submenuProperty.details != null) {
                binding.details.visibility = View.VISIBLE
                binding.details.text = submenuProperty.details.invoke()
            } else if (submenuProperty.detailsFlow != null) {
                binding.details.visibility = View.VISIBLE
                viewLifecycle.lifecycleScope.launch {
                    viewLifecycle.repeatOnLifecycle(Lifecycle.State.STARTED) {
                        submenuProperty.detailsFlow.collect { binding.details.text = it }
                    }
                }
            } else {
                binding.details.visibility = View.GONE
            }
        }
    }

    inner class InstallablePropertyViewHolder(val binding: CardInstallableIconBinding) :
        GamePropertyViewHolder(binding.root) {
        override fun bind(property: GameProperty) {
            val installableProperty = property as InstallableProperty

            binding.title.setText(installableProperty.titleId)
            binding.description.setText(installableProperty.descriptionId)
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.context.resources,
                    installableProperty.iconId,
                    binding.icon.context.theme
                )
            )

            if (installableProperty.install != null) {
                binding.buttonInstall.visibility = View.VISIBLE
                binding.buttonInstall.setOnClickListener { installableProperty.install.invoke() }
            }
            if (installableProperty.export != null) {
                binding.buttonExport.visibility = View.VISIBLE
                binding.buttonExport.setOnClickListener { installableProperty.export.invoke() }
            }
        }
    }

    enum class PropertyType {
        Submenu,
        Installable
    }
}
