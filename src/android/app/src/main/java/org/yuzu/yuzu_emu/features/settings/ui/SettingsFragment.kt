// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.view.ViewGroup.MarginLayoutParams
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.divider.MaterialDividerItemDecoration
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.FragmentSettingsBinding
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.SettingsViewModel

class SettingsFragment : Fragment() {
    private lateinit var presenter: SettingsFragmentPresenter
    private var settingsAdapter: SettingsAdapter? = null

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<SettingsFragmentArgs>()

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSettingsBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        settingsAdapter = SettingsAdapter(this, requireContext())
        presenter = SettingsFragmentPresenter(
            settingsViewModel,
            settingsAdapter!!,
            args.menuTag,
            args.game?.gameId ?: ""
        )

        val dividerDecoration = MaterialDividerItemDecoration(
            requireContext(),
            LinearLayoutManager.VERTICAL
        )
        dividerDecoration.isLastItemDecorated = false
        binding.listSettings.apply {
            adapter = settingsAdapter
            layoutManager = LinearLayoutManager(requireContext())
            addItemDecoration(dividerDecoration)
        }

        binding.toolbarSettings.setNavigationOnClickListener {
            settingsViewModel.setShouldNavigateBack(true)
        }

        settingsViewModel.toolbarTitle.observe(viewLifecycleOwner) {
            if (it.isNotEmpty()) binding.toolbarSettingsLayout.title = it
        }

        settingsViewModel.shouldReloadSettingsList.observe(viewLifecycleOwner) {
            if (it) {
                settingsViewModel.setShouldReloadSettingsList(false)
                presenter.loadSettingsList()
            }
        }

        settingsViewModel.isUsingSearch.observe(viewLifecycleOwner) {
            if (it) {
                reenterTransition = MaterialSharedAxis(MaterialSharedAxis.Z, true)
                exitTransition = MaterialSharedAxis(MaterialSharedAxis.Z, false)
            } else {
                reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
                exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
            }
        }

        if (args.menuTag == SettingsFile.FILE_NAME_CONFIG) {
            binding.toolbarSettings.inflateMenu(R.menu.menu_settings)
            binding.toolbarSettings.setOnMenuItemClickListener {
                when (it.itemId) {
                    R.id.action_search -> {
                        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.Z, true)
                        exitTransition = MaterialSharedAxis(MaterialSharedAxis.Z, false)
                        view.findNavController()
                            .navigate(R.id.action_settingsFragment_to_settingsSearchFragment)
                        true
                    }

                    else -> false
                }
            }
        }

        presenter.onViewCreated()

        setInsets()
    }

    override fun onResume() {
        super.onResume()
        settingsViewModel.setIsUsingSearch(false)
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            val sideMargin = resources.getDimensionPixelSize(R.dimen.spacing_medlarge)
            val mlpSettingsList = binding.listSettings.layoutParams as MarginLayoutParams
            mlpSettingsList.leftMargin = sideMargin + leftInsets
            mlpSettingsList.rightMargin = sideMargin + rightInsets
            binding.listSettings.layoutParams = mlpSettingsList
            binding.listSettings.updatePadding(
                bottom = barInsets.bottom
            )

            val mlpAppBar = binding.appbarSettings.layoutParams as MarginLayoutParams
            mlpAppBar.leftMargin = leftInsets
            mlpAppBar.rightMargin = rightInsets
            binding.appbarSettings.layoutParams = mlpAppBar
            windowInsets
        }
    }
}
