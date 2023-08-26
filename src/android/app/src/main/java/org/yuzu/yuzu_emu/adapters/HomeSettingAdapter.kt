// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.text.TextUtils
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.core.content.res.ResourcesCompat
import androidx.lifecycle.LifecycleOwner
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.CardHomeOptionBinding
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.model.HomeSetting

class HomeSettingAdapter(
    private val activity: AppCompatActivity,
    private val viewLifecycle: LifecycleOwner,
    var options: List<HomeSetting>
) :
    RecyclerView.Adapter<HomeSettingAdapter.HomeOptionViewHolder>(),
    View.OnClickListener {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HomeOptionViewHolder {
        val binding =
            CardHomeOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
        binding.root.setOnClickListener(this)
        return HomeOptionViewHolder(binding)
    }

    override fun getItemCount(): Int {
        return options.size
    }

    override fun onBindViewHolder(holder: HomeOptionViewHolder, position: Int) {
        holder.bind(options[position])
    }

    override fun onClick(view: View) {
        val holder = view.tag as HomeOptionViewHolder
        if (holder.option.isEnabled.invoke()) {
            holder.option.onClick.invoke()
        } else {
            MessageDialogFragment.newInstance(
                holder.option.disabledTitleId,
                holder.option.disabledMessageId
            ).show(activity.supportFragmentManager, MessageDialogFragment.TAG)
        }
    }

    inner class HomeOptionViewHolder(val binding: CardHomeOptionBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var option: HomeSetting

        init {
            itemView.tag = this
        }

        fun bind(option: HomeSetting) {
            this.option = option
            binding.optionTitle.text = activity.resources.getString(option.titleId)
            binding.optionDescription.text = activity.resources.getString(option.descriptionId)
            binding.optionIcon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    activity.resources,
                    option.iconId,
                    activity.theme
                )
            )

            when (option.titleId) {
                R.string.get_early_access ->
                    binding.optionLayout.background =
                        ContextCompat.getDrawable(
                            binding.optionCard.context,
                            R.drawable.premium_background
                        )
            }

            if (!option.isEnabled.invoke()) {
                binding.optionTitle.alpha = 0.5f
                binding.optionDescription.alpha = 0.5f
                binding.optionIcon.alpha = 0.5f
            }

            option.details.observe(viewLifecycle) { updateOptionDetails(it) }
            binding.optionDetail.postDelayed(
                {
                    binding.optionDetail.ellipsize = TextUtils.TruncateAt.MARQUEE
                    binding.optionDetail.isSelected = true
                },
                3000
            )
        }

        private fun updateOptionDetails(detailString: String) {
            if (detailString.isNotEmpty()) {
                binding.optionDetail.text = detailString
                binding.optionDetail.visibility = View.VISIBLE
            }
        }
    }
}
