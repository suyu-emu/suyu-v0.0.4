// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.content.Context
import android.view.LayoutInflater
import android.view.ViewGroup
import androidx.recyclerview.widget.DiffUtil
import androidx.recyclerview.widget.ListAdapter
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.ListItemFreedrenoVariableBinding
import org.yuzu.yuzu_emu.fragments.FreedrenoVariable
import org.yuzu.yuzu_emu.utils.NativeFreedrenoConfig

/**
 * Adapter for displaying currently set Freedreno environment variables in a list.
 */
class FreedrenoVariableAdapter(
    private val context: Context,
    private val onItemClicked: (FreedrenoVariable, () -> Unit) -> Unit
) : ListAdapter<FreedrenoVariable, FreedrenoVariableAdapter.VariableViewHolder>(DiffCallback) {

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): VariableViewHolder {
        val binding = ListItemFreedrenoVariableBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return VariableViewHolder(binding)
    }

    override fun onBindViewHolder(holder: VariableViewHolder, position: Int) {
        holder.bind(getItem(position))
    }

    inner class VariableViewHolder(private val binding: ListItemFreedrenoVariableBinding) :
        RecyclerView.ViewHolder(binding.root) {

        fun bind(variable: FreedrenoVariable) {
            binding.variableName.text = variable.name
            binding.variableValue.text = variable.value

            binding.buttonDelete.setOnClickListener {
                onItemClicked(variable) {
                    NativeFreedrenoConfig.clearFreedrenoEnv(variable.name)
                }
            }
        }
    }

    companion object {
        private val DiffCallback = object : DiffUtil.ItemCallback<FreedrenoVariable>() {
            override fun areItemsTheSame(oldItem: FreedrenoVariable, newItem: FreedrenoVariable): Boolean =
                oldItem.name == newItem.name

            override fun areContentsTheSame(oldItem: FreedrenoVariable, newItem: FreedrenoVariable): Boolean =
                oldItem == newItem
        }
    }
}
