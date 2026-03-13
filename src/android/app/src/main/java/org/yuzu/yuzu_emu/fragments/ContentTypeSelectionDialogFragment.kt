// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.net.Uri
import android.os.Bundle
import androidx.activity.result.contract.ActivityResultContracts
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import androidx.preference.PreferenceManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.YuzuApplication
import org.yuzu.yuzu_emu.model.AddonViewModel
import org.yuzu.yuzu_emu.utils.InstallableActions

class ContentTypeSelectionDialogFragment : DialogFragment() {
    private val addonViewModel: AddonViewModel by activityViewModels()

    private val preferences get() =
        PreferenceManager.getDefaultSharedPreferences(YuzuApplication.appContext)

    private var selectedItem = 0

    private val installGameUpdateLauncher =
        registerForActivityResult(ActivityResultContracts.OpenMultipleDocuments()) { documents ->
            if (documents.isEmpty()) {
                return@registerForActivityResult
            }

            val game = addonViewModel.game
            if (game == null) {
                installContent(documents)
                return@registerForActivityResult
            }

            ProgressDialogFragment.newInstance(
                requireActivity(),
                R.string.verifying_content,
                false
            ) { _, _ ->
                var updatesMatchProgram = true
                for (document in documents) {
                    val valid = NativeLibrary.doesUpdateMatchProgram(
                        game.programId,
                        document.toString()
                    )
                    if (!valid) {
                        updatesMatchProgram = false
                        break
                    }
                }

                requireActivity().runOnUiThread {
                    if (updatesMatchProgram) {
                        installContent(documents)
                    } else {
                        MessageDialogFragment.newInstance(
                            requireActivity(),
                            titleId = R.string.content_install_notice,
                            descriptionId = R.string.content_install_notice_description,
                            positiveAction = { installContent(documents) },
                            negativeAction = {}
                        ).show(parentFragmentManager, MessageDialogFragment.TAG)
                    }
                }
                return@newInstance Any()
            }.show(parentFragmentManager, ProgressDialogFragment.TAG)
        }

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val launchOptions =
            arrayOf(getString(R.string.updates_and_dlc), getString(R.string.mods_and_cheats))

        if (savedInstanceState != null) {
            selectedItem = savedInstanceState.getInt(SELECTED_ITEM)
        }

        return MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.select_content_type)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                when (selectedItem) {
                    0 -> installGameUpdateLauncher.launch(arrayOf("*/*"))
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
            .setSingleChoiceItems(launchOptions, selectedItem) { _: DialogInterface, i: Int ->
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

    private fun installContent(documents: List<Uri>) {
        InstallableActions.installContent(
            activity = requireActivity(),
            fragmentManager = parentFragmentManager,
            addonViewModel = addonViewModel,
            documents = documents
        )
    }
}
