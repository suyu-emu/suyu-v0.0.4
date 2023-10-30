// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.content.res.ResourcesCompat
import androidx.fragment.app.Fragment
import androidx.navigation.fragment.findNavController
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.HomeNavigationDirections
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.DialogListItemBinding
import org.yuzu.yuzu_emu.model.CabinetMode
import org.yuzu.yuzu_emu.adapters.CabinetLauncherDialogAdapter.CabinetModeViewHolder
import org.yuzu.yuzu_emu.model.AppletInfo
import org.yuzu.yuzu_emu.model.Game

class CabinetLauncherDialogAdapter(val fragment: Fragment) :
    RecyclerView.Adapter<CabinetModeViewHolder>(),
    View.OnClickListener {
    private val cabinetModes = CabinetMode.values().copyOfRange(1, CabinetMode.values().size)

    override fun onCreateViewHolder(parent: ViewGroup, viewType: Int): CabinetModeViewHolder {
        DialogListItemBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .apply { root.setOnClickListener(this@CabinetLauncherDialogAdapter) }
            .also { return CabinetModeViewHolder(it) }
    }

    override fun getItemCount(): Int = cabinetModes.size

    override fun onBindViewHolder(holder: CabinetModeViewHolder, position: Int) =
        holder.bind(cabinetModes[position])

    override fun onClick(view: View) {
        val mode = (view.tag as CabinetModeViewHolder).cabinetMode
        val appletPath = NativeLibrary.getAppletLaunchPath(AppletInfo.Cabinet.entryId)
        NativeLibrary.setCurrentAppletId(AppletInfo.Cabinet.appletId)
        NativeLibrary.setCabinetMode(mode.id)
        val appletGame = Game(
            title = YuzuApplication.appContext.getString(R.string.cabinet_applet),
            path = appletPath
        )
        val action = HomeNavigationDirections.actionGlobalEmulationActivity(appletGame)
        fragment.findNavController().navigate(action)
    }

    inner class CabinetModeViewHolder(val binding: DialogListItemBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var cabinetMode: CabinetMode

        init {
            itemView.tag = this
        }

        fun bind(cabinetMode: CabinetMode) {
            this.cabinetMode = cabinetMode
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.context.resources,
                    cabinetMode.iconId,
                    binding.icon.context.theme
                )
            )
            binding.title.setText(cabinetMode.titleId)
        }
    }
}
