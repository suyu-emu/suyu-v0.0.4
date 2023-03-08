package org.yuzu.yuzu_emu.features.settings.ui.viewholder

import android.view.View
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem
import org.yuzu.yuzu_emu.features.settings.ui.SettingsAdapter

abstract class SettingViewHolder(itemView: View, protected val adapter: SettingsAdapter) :
    RecyclerView.ViewHolder(itemView), View.OnClickListener {

    init {
        itemView.setOnClickListener(this)
        findViews(itemView)
    }

    /**
     * Gets handles to all this ViewHolder's child views using their XML-defined identifiers.
     *
     * @param root The newly inflated top-level view.
     */
    protected abstract fun findViews(root: View)

    /**
     * Called by the adapter to set this ViewHolder's child views to display the list item
     * it must now represent.
     *
     * @param item The list item that should be represented by this ViewHolder.
     */
    abstract fun bind(item: SettingsItem)

    /**
     * Called when this ViewHolder's view is clicked on. Implementations should usually pass
     * this event up to the adapter.
     *
     * @param clicked The view that was clicked on.
     */
    abstract override fun onClick(clicked: View)
}
