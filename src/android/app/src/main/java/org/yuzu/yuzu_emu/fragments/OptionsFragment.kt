// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.content.DialogInterface
import android.content.Intent
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.HomeOptionAdapter
import org.yuzu.yuzu_emu.databinding.FragmentOptionsBinding
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.HomeOption
import org.yuzu.yuzu_emu.ui.main.MainActivity
import org.yuzu.yuzu_emu.utils.GpuDriverHelper

class OptionsFragment : Fragment() {
    private var _binding: FragmentOptionsBinding? = null
    private val binding get() = _binding!!

    private lateinit var mainActivity: MainActivity

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentOptionsBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        mainActivity = requireActivity() as MainActivity

        val optionsList: List<HomeOption> = listOf(
            HomeOption(
                R.string.add_games,
                R.string.add_games_description,
                R.drawable.ic_add
            ) { mainActivity.getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data) },
            HomeOption(
                R.string.install_prod_keys,
                R.string.install_prod_keys_description,
                R.drawable.ic_unlock
            ) { mainActivity.getProdKey.launch(arrayOf("*/*")) },
            HomeOption(
                R.string.install_amiibo_keys,
                R.string.install_amiibo_keys_description,
                R.drawable.ic_nfc
            ) { mainActivity.getAmiiboKey.launch(arrayOf("*/*")) },
            HomeOption(
                R.string.install_gpu_driver,
                R.string.install_gpu_driver_description,
                R.drawable.ic_input
            ) { driverInstaller() },
            HomeOption(
                R.string.settings,
                R.string.settings_description,
                R.drawable.ic_settings
            ) { SettingsActivity.launch(requireContext(), SettingsFile.FILE_NAME_CONFIG, "") }
        )

        binding.optionsList.apply {
            layoutManager = LinearLayoutManager(requireContext())
            adapter = HomeOptionAdapter(requireActivity() as AppCompatActivity, optionsList)
        }

        setInsets()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        _binding = null
    }

    private fun driverInstaller() {
        // Get the driver name for the dialog message.
        var driverName = GpuDriverHelper.customDriverName
        if (driverName == null) {
            driverName = getString(R.string.system_gpu_driver)
        }

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(getString(R.string.select_gpu_driver_title))
            .setMessage(driverName)
            .setNegativeButton(android.R.string.cancel, null)
            .setPositiveButton(R.string.select_gpu_driver_default) { _: DialogInterface?, _: Int ->
                GpuDriverHelper.installDefaultDriver(requireContext())
                Toast.makeText(
                    requireContext(),
                    R.string.select_gpu_driver_use_default,
                    Toast.LENGTH_SHORT
                ).show()
            }
            .setNeutralButton(R.string.select_gpu_driver_install) { _: DialogInterface?, _: Int ->
                mainActivity.getDriver.launch(arrayOf("application/zip"))
            }
            .show()
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(binding.scrollViewOptions) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.setPadding(
                insets.left,
                insets.top,
                insets.right,
                insets.bottom + resources.getDimensionPixelSize(R.dimen.spacing_navigation)
            )
            windowInsets
        }
}
