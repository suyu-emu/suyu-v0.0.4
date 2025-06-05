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
import androidx.transition.AutoTransition
import androidx.transition.ChangeBounds
import androidx.transition.Fade
import androidx.transition.TransitionManager
import androidx.transition.TransitionSet
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import org.yuzu.yuzu_emu.model.DriverViewModel

class DriverGroupAdapter(
    private val activity: FragmentActivity,
    private val driverViewModel: DriverViewModel
) : RecyclerView.Adapter<DriverGroupAdapter.DriverGroupViewHolder>() {
    private var driverGroups: List<DriverGroup> = emptyList()

    inner class DriverGroupViewHolder(
        private val binding: ItemDriverGroupBinding
    ) : RecyclerView.ViewHolder(binding.root) {
        fun bind(group: DriverGroup) {
            val onClick = {
                TransitionManager.beginDelayedTransition(
                    binding.root,
                    TransitionSet().addTransition(Fade()).addTransition(ChangeBounds())
                        .setDuration(200)
                )
                val isVisible = binding.recyclerReleases.isVisible

                binding.recyclerReleases.visibility = if (isVisible) View.GONE else View.VISIBLE
                binding.imageDropdownArrow.rotation = if (isVisible) 0f else 180f

                if (!isVisible && binding.recyclerReleases.adapter == null) {
                    CoroutineScope(Dispatchers.Main).launch {
                        binding.recyclerReleases.layoutManager =
                            LinearLayoutManager(binding.root.context)
                        binding.recyclerReleases.adapter =
                            ReleaseAdapter(group.releases, activity, driverViewModel)

                        binding.recyclerReleases.addItemDecoration(
                            SpacingItemDecoration(
                                (activity.resources.displayMetrics.density * 8).toInt()
                            )
                        )
                    }
                }
            }

            binding.textGroupName.text = group.name
            binding.textGroupName.setOnClickListener { onClick() }

            binding.imageDropdownArrow.setOnClickListener { onClick() }
        }
    }

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): DriverGroupViewHolder {
        val binding = ItemDriverGroupBinding.inflate(
            LayoutInflater.from(parent.context), parent, false
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