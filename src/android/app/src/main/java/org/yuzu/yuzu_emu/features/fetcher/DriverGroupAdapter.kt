// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.features.fetcher

import android.annotation.SuppressLint
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.ItemDriverGroupBinding
import org.yuzu.yuzu_emu.fragments.DriverFetcherFragment.DriverGroup
import androidx.core.view.isVisible
import androidx.fragment.app.FragmentActivity
import androidx.transition.ChangeBounds
import androidx.transition.Fade
import androidx.transition.TransitionManager
import androidx.transition.TransitionSet
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.model.DriverViewModel

class DriverGroupAdapter(
    private val activity: FragmentActivity,
    private val driverViewModel: DriverViewModel
) : RecyclerView.Adapter<DriverGroupAdapter.DriverGroupViewHolder>() {
    private var driverGroups: List<DriverGroup> = emptyList()
    private val adapterJobs = mutableMapOf<Int, Job>()

    inner class DriverGroupViewHolder(
        private val binding: ItemDriverGroupBinding
    ) : RecyclerView.ViewHolder(binding.root) {
        fun bind(group: DriverGroup) {
            binding.textGroupName.text = group.name

            if (binding.recyclerReleases.layoutManager == null) {
                binding.recyclerReleases.layoutManager = LinearLayoutManager(activity)
                binding.recyclerReleases.addItemDecoration(
                    SpacingItemDecoration(
                        (activity.resources.displayMetrics.density * 8).toInt()
                    )
                )
            }

            val onClick = {
                adapterJobs[bindingAdapterPosition]?.cancel()

                TransitionManager.beginDelayedTransition(
                    binding.root,
                    TransitionSet().addTransition(Fade()).addTransition(ChangeBounds())
                        .setDuration(200)
                )

                val isVisible = binding.recyclerReleases.isVisible

                if (!isVisible && binding.recyclerReleases.adapter == null) {
                    val job = CoroutineScope(Dispatchers.Main).launch {
                        // It prevents blocking the ui thread.
                        var adapter: ReleaseAdapter?

                        withContext(Dispatchers.IO) {
                            adapter = ReleaseAdapter(group.releases, activity, driverViewModel)
                        }

                        binding.recyclerReleases.adapter = adapter
                    }

                    adapterJobs[bindingAdapterPosition] = job
                }

                binding.recyclerReleases.visibility = if (isVisible) View.GONE else View.VISIBLE
                binding.imageDropdownArrow.rotation = if (isVisible) 0f else 180f
            }

            binding.textGroupName.setOnClickListener { onClick() }
            binding.imageDropdownArrow.setOnClickListener { onClick() }
        }

        fun clear() {
            adapterJobs[bindingAdapterPosition]?.cancel()
            adapterJobs.remove(bindingAdapterPosition)
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DriverGroupViewHolder {
        val binding = ItemDriverGroupBinding.inflate(
            LayoutInflater.from(parent.context),
            parent,
            false
        )
        return DriverGroupViewHolder(binding)
    }

    override fun onBindViewHolder(holder: DriverGroupViewHolder, position: Int) {
        holder.bind(driverGroups[position])
    }

    override fun getItemCount(): Int = driverGroups.size

    @SuppressLint("NotifyDataSetChanged")
    fun updateDriverGroups(newDriverGroups: List<DriverGroup>) {
        driverGroups = newDriverGroups
        notifyDataSetChanged()
    }
}
