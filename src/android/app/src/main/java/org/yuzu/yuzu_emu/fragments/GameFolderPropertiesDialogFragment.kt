// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.app.Dialog
import android.content.DialogInterface
import android.os.Bundle
import android.view.View
import androidx.fragment.app.DialogFragment
import androidx.fragment.app.activityViewModels
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.DialogFolderPropertiesBinding
import org.yuzu.yuzu_emu.model.DirectoryType
import org.yuzu.yuzu_emu.model.GameDir
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.utils.NativeConfig
import org.yuzu.yuzu_emu.utils.SerializableHelper.parcelable

class GameFolderPropertiesDialogFragment : DialogFragment() {
    private val gamesViewModel: GamesViewModel by activityViewModels()

    private var deepScan = false

    override fun onCreateDialog(savedInstanceState: Bundle?): Dialog {
        val binding = DialogFolderPropertiesBinding.inflate(layoutInflater)
        val gameDir = requireArguments().parcelable<GameDir>(GAME_DIR)!!

        // Hide deepScan for external content, do automatically
        if (gameDir.type == DirectoryType.EXTERNAL_CONTENT) {
            binding.deepScanSwitch.visibility = View.GONE
        } else {
            // Restore checkbox state for game dirs
            binding.deepScanSwitch.isChecked =
                savedInstanceState?.getBoolean(DEEP_SCAN) ?: gameDir.deepScan

            deepScan = binding.deepScanSwitch.isChecked
            binding.deepScanSwitch.setOnClickListener {
                deepScan = binding.deepScanSwitch.isChecked
            }
        }

        return MaterialAlertDialogBuilder(requireContext())
            .setView(binding.root)
            .setTitle(R.string.game_folder_properties)
            .setPositiveButton(android.R.string.ok) { _: DialogInterface, _: Int ->
                val folderIndex = gamesViewModel.folders.value.indexOf(gameDir)
                if (folderIndex != -1) {
                    if (gameDir.type == DirectoryType.GAME) {
                        gamesViewModel.folders.value[folderIndex].deepScan =
                            binding.deepScanSwitch.isChecked
                    }
                    gamesViewModel.updateGameDirs()
                }
            }
            .setNegativeButton(android.R.string.cancel, null)
            .show()
    }

    override fun onStop() {
        super.onStop()
        NativeConfig.saveGlobalConfig()
    }

    override fun onSaveInstanceState(outState: Bundle) {
        super.onSaveInstanceState(outState)
        outState.putBoolean(DEEP_SCAN, deepScan)
    }

    companion object {
        const val TAG = "GameFolderPropertiesDialogFragment"

        private const val GAME_DIR = "GameDir"

        private const val DEEP_SCAN = "DeepScan"

        fun newInstance(gameDir: GameDir): GameFolderPropertiesDialogFragment {
            val args = Bundle()
            args.putParcelable(GAME_DIR, gameDir)
            val fragment = GameFolderPropertiesDialogFragment()
            fragment.arguments = args
            return fragment
        }
    }
}
