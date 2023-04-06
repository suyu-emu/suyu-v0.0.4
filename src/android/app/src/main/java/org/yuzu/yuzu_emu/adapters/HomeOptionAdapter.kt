package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.res.ResourcesCompat
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.databinding.CardHomeOptionBinding
import org.yuzu.yuzu_emu.model.HomeOption

class HomeOptionAdapter(private val activity: AppCompatActivity, var options: List<HomeOption>) :
    RecyclerView.Adapter<HomeOptionAdapter.HomeOptionViewHolder>(),
    View.OnClickListener {
    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): HomeOptionViewHolder {
        val binding = CardHomeOptionBinding.inflate(LayoutInflater.from(parent.context), parent, false)
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
        holder.option.onClick.invoke()
    }

    inner class HomeOptionViewHolder(val binding: CardHomeOptionBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var option: HomeOption

        init {
            itemView.tag = this
        }

        fun bind(option: HomeOption) {
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
        }
    }
}
