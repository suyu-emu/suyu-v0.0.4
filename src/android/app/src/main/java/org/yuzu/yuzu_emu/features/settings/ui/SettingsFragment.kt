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
import androidx.fragment.app.FragmentActivity
import androidx.recyclerview.widget.LinearLayoutManager
import androidx.recyclerview.widget.RecyclerView
import com.google.android.material.divider.MaterialDividerItemDecoration
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.features.settings.model.Setting
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.view.SettingsItem

class SettingsFragment : Fragment(), SettingsFragmentView {
    override lateinit var fragmentActivity: FragmentActivity

    private val presenter = SettingsFragmentPresenter(this)
    private var activityView: SettingsActivityView? = null
    private var adapter: SettingsAdapter? = null

    private lateinit var recyclerView: RecyclerView

    override fun onAttach(context: Context) {
        super.onAttach(context)
        activityView = context as SettingsActivityView
        fragmentActivity = requireActivity()
        presenter.onAttach()
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        val menuTag = requireArguments().getString(ARGUMENT_MENU_TAG)
        val gameId = requireArguments().getString(ARGUMENT_GAME_ID)
        adapter = SettingsAdapter(this, requireActivity())
        presenter.onCreate(menuTag!!, gameId!!)
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View? {
        return inflater.inflate(R.layout.fragment_settings, container, false)
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val manager = LinearLayoutManager(activity)
        recyclerView = view.findViewById(R.id.list_settings)
        recyclerView.adapter = adapter
        recyclerView.layoutManager = manager
        val dividerDecoration = MaterialDividerItemDecoration(requireContext(), LinearLayoutManager.VERTICAL)
        dividerDecoration.isLastItemDecorated = false
        recyclerView.addItemDecoration(dividerDecoration)
        val activity = activity as SettingsActivityView?
        presenter.onViewCreated(activity!!.settings)

        setInsets()
    }

    override fun onDetach() {
        super.onDetach()
        activityView = null
        if (adapter != null) {
            adapter!!.closeDialog()
        }
    }

    override fun onSettingsFileLoaded(settings: Settings?) {
        presenter.setSettings(settings)
    }

    override fun passSettingsToActivity(settings: Settings) {
        if (activityView != null) {
            activityView!!.settings = settings
        }
    }

    override fun showSettingsList(settingsList: ArrayList<SettingsItem>) {
        adapter!!.setSettings(settingsList)
    }

    override fun loadDefaultSettings() {
        presenter.loadDefaultSettings()
    }

    override fun loadSubMenu(menuKey: String) {
        activityView!!.showSettingsFragment(
            menuKey,
            true,
            requireArguments().getString(ARGUMENT_GAME_ID)!!
        )
    }

    override fun showToastMessage(message: String?, is_long: Boolean) {
        activityView!!.showToastMessage(message!!, is_long)
    }

    override fun putSetting(setting: Setting) {
        presenter.putSetting(setting)
    }

    override fun onSettingChanged() {
        activityView!!.onSettingChanged()
    }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(recyclerView) { view: View, windowInsets: WindowInsetsCompat ->
            val insets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            view.updatePadding(bottom = insets.bottom)
            windowInsets
        }
    }

    companion object {
        private const val ARGUMENT_MENU_TAG = "menu_tag"
        private const val ARGUMENT_GAME_ID = "game_id"

        @JvmStatic
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
