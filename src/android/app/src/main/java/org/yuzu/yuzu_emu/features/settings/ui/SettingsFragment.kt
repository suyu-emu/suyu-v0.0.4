// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.content.Context
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.divider.MaterialDividerItemDecoration
import org.yuzu.yuzu_emu.databinding.FragmentSettingsBinding
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem

class SettingsFragment : Fragment(), SettingsFragmentView {
    override var activityView: SettingsActivityView? = null

    private val fragmentPresenter = SettingsFragmentPresenter(this)
    private var settingsAdapter: SettingsAdapter? = null

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    override fun onAttach(context: Context) {
        super.onAttach(context)
        activityView = requireActivity() as SettingsActivityView
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val menuTag = requireArguments().getString(ARGUMENT_MENU_TAG)
        val gameId = requireArguments().getString(ARGUMENT_GAME_ID)
        fragmentPresenter.onCreate(menuTag!!, gameId!!)
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
        settingsAdapter = SettingsAdapter(this, requireActivity())
        val dividerDecoration = MaterialDividerItemDecoration(
            requireContext(),
            LinearLayoutManager.VERTICAL
        )
        dividerDecoration.isLastItemDecorated = false
        binding.listSettings.apply {
            adapter = settingsAdapter
            layoutManager = LinearLayoutManager(activity)
            addItemDecoration(dividerDecoration)
        }
        fragmentPresenter.onViewCreated()

        setInsets()
    }

    override fun onDetach() {
        super.onDetach()
        activityView = null
        if (settingsAdapter != null) {
            settingsAdapter!!.closeDialog()
        }
    }

    override fun showSettingsList(settingsList: ArrayList<SettingsItem>) {
        settingsAdapter!!.setSettingsList(settingsList)
    }

    override fun loadSettingsList() {
        fragmentPresenter.loadSettingsList()
    }

    override fun loadSubMenu(menuKey: String) {
        activityView!!.showSettingsFragment(
            menuKey,
            true,
            requireArguments().getString(ARGUMENT_GAME_ID)!!
        )
    }

    override fun onSettingChanged() {
        activityView!!.onSettingChanged()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.listSettings
        ) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updatePadding(bottom = insets.bottom)
            windowInsets
        }
    }

    companion object {
        private const val ARGUMENT_MENU_TAG = "menu_tag"
        private const val ARGUMENT_GAME_ID = "game_id"

        fun newInstance(menuTag: String?, gameId: String?): Fragment {
            val fragment = SettingsFragment()
            val arguments = Bundle()
            arguments.putString(ARGUMENT_MENU_TAG, menuTag)
            arguments.putString(ARGUMENT_GAME_ID, gameId)
            fragment.arguments = arguments
            return fragment
        }
    }
}
