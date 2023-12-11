// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.adapters

import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.core.content.res.ResourcesCompat
import androidx.fragment.app.FragmentActivity
import androidx.navigation.findNavController
import androidx.recyclerview.widget.RecyclerView
import org.yuzu.yuzu_emu.HomeNavigationDirections
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.databinding.CardSimpleOutlinedBinding
import org.yuzu.yuzu_emu.model.Applet
import org.yuzu.yuzu_emu.model.AppletInfo
import org.yuzu.yuzu_emu.model.Game

class AppletAdapter(val activity: FragmentActivity, var applets: List<Applet>) :
    RecyclerView.Adapter<AppletAdapter.AppletViewHolder>(),
    View.OnClickListener {

    override fun onCreateViewHolder(
        parent: ViewGroup,
        viewType: Int
    ): AppletAdapter.AppletViewHolder {
        CardSimpleOutlinedBinding.inflate(LayoutInflater.from(parent.context), parent, false)
            .apply { root.setOnClickListener(this@AppletAdapter) }
            .also { return AppletViewHolder(it) }
    }

    override fun onBindViewHolder(holder: AppletViewHolder, position: Int) =
        holder.bind(applets[position])

    override fun getItemCount(): Int = applets.size

    override fun onClick(view: View) {
        val applet = (view.tag as AppletViewHolder).applet
        val appletPath = NativeLibrary.getAppletLaunchPath(applet.appletInfo.entryId)
        if (appletPath.isEmpty()) {
            Toast.makeText(
                YuzuApplication.appContext,
                R.string.applets_error_applet,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        if (applet.appletInfo == AppletInfo.Cabinet) {
            view.findNavController()
                .navigate(R.id.action_appletLauncherFragment_to_cabinetLauncherDialogFragment)
            return
        }

        NativeLibrary.setCurrentAppletId(applet.appletInfo.appletId)
        val appletGame = Game(
            title = YuzuApplication.appContext.getString(applet.titleId),
            path = appletPath
        )
        val action = HomeNavigationDirections.actionGlobalEmulationActivity(appletGame)
        view.findNavController().navigate(action)
    }

    inner class AppletViewHolder(val binding: CardSimpleOutlinedBinding) :
        RecyclerView.ViewHolder(binding.root) {
        lateinit var applet: Applet

        init {
            itemView.tag = this
        }

        fun bind(applet: Applet) {
            this.applet = applet

            binding.title.setText(applet.titleId)
            binding.description.setText(applet.descriptionId)
            binding.icon.setImageDrawable(
                ResourcesCompat.getDrawable(
                    binding.icon.context.resources,
                    applet.iconId,
                    binding.icon.context.theme
                )
            )
        }
    }
}
