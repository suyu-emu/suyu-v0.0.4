// SPDX-FileCopyrightText: 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.suyu.suyu_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.suyu.suyu_emu.R
import org.suyu.suyu_emu.SuyuApplication
import org.suyu.suyu_emu.model.AddonViewModel
import org.suyu.suyu_emu.ui.main.MainActivity

class ContentTypeSelectionDialogFragment : DialogFragment() {
    private val addonViewModel: AddonViewModel by activityViewModels()

    private val preferences get() =
        PreferenceManager.getDefaultSharedPreferences(SuyuApplication.appContext)

    private var selectedItem = 0

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val launchOptions =
            arrayOf(getString(R.string.updates_and_dlc), getString(R.string.mods_and_cheats))

        if (savedInstanceState != null) {
            selectedItem = savedInstanceState.getInt(SELECTED_ITEM)
        }

        val mainActivity = requireActivity() as MainActivity
        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.select_content_type)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                when (selectedItem) {
                    0 -> mainActivity.installGameUpdate.launch(arrayOf("*/*"))
                    else -> {
                        if (!preferences.getBoolean(MOD_NOTICE_SEEN, false)) {
                            preferences.edit().putBoolean(MOD_NOTICE_SEEN, true).apply()
                            addonViewModel.showModNoticeDialog(true)
                            return@setPositiveButton
                        }
                        addonViewModel.showModInstallPicker(true)
                    }
                }
            }
            .setSingleChoiceItems(launchOptions, 0) { _: DialogInterface, i: Int ->
                selectedItem = i
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putInt(SELECTED_ITEM, selectedItem)
    }

    companion object {
        const val TAG = "ContentTypeSelectionDialogFragment"

        private const val SELECTED_ITEM = "SelectedItem"
        private const val MOD_NOTICE_SEEN = "ModNoticeSeen"
    }
}
