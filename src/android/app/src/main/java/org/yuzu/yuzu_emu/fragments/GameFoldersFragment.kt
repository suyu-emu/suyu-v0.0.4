// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

package org.yuzu.yuzu_emu.fragments

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import android.view.LayoutInflater
import android.view.View
import android.view.ViewGroup
import android.widget.Toast
import androidx.activity.result.contract.ActivityResultContracts
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.updatePadding
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.recyclerview.widget.GridLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.FolderAdapter
import org.yuzu.yuzu_emu.databinding.FragmentFoldersBinding
import org.yuzu.yuzu_emu.model.DirectoryType
import org.yuzu.yuzu_emu.model.GameDir
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeViewModel
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins
import org.yuzu.yuzu_emu.utils.collect

class GameFoldersFragment : Fragment() {
    private var _binding: FragmentFoldersBinding? = null
    private val binding get() = _binding!!

    private val homeViewModel: HomeViewModel by activityViewModels()
    private val gamesViewModel: GamesViewModel by activityViewModels()

    private val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                processGamesDir(result)
            }
        }

    private val getExternalContentDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result != null) {
                processExternalContentDir(result)
            }
        }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)

        gamesViewModel.onOpenGameFoldersFragment()
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentFoldersBinding.inflate(inflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        homeViewModel.setStatusBarShadeVisibility(visible = false)

        binding.toolbarFolders.setNavigationOnClickListener {
            requireActivity().onBackPressedDispatcher.onBackPressed()
        }

        binding.listFolders.apply {
            layoutManager = GridLayoutManager(
                requireContext(),
                resources.getInteger(R.integer.grid_columns)
            )
            adapter = FolderAdapter(requireActivity(), gamesViewModel)
        }

        gamesViewModel.folders.collect(viewLifecycleOwner) {
            (binding.listFolders.adapter as FolderAdapter).submitList(it)
        }

        binding.buttonAdd.setOnClickListener {
            // Show a model to choose between Game and External Content
            val options = arrayOf(
                getString(R.string.games),
                getString(R.string.external_content)
            )

            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.add_folders)
                .setItems(options) { _, which ->
                    when (which) {
                        0 -> { // Game Folder
                            getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data)
                        }
                        1 -> { // External Content Folder
                            getExternalContentDirectory.launch(null)
                        }
                    }
                }
                .show()
        }

        setInsets()
    }

    override fun onStop() {
        super.onStop()
        gamesViewModel.onCloseGameFoldersFragment()
    }

    private fun processGamesDir(result: Uri) {
        requireContext().contentResolver.takePersistableUriPermission(
            result,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )

        val uriString = result.toString()
        val folder = gamesViewModel.folders.value.firstOrNull { it.uriString == uriString }
        if (folder != null) {
            Toast.makeText(
                requireContext().applicationContext,
                R.string.folder_already_added,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        AddGameFolderDialogFragment.newInstance(uriString, calledFromGameFragment = false)
            .show(parentFragmentManager, AddGameFolderDialogFragment.TAG)
    }

    private fun processExternalContentDir(result: Uri) {
        requireContext().contentResolver.takePersistableUriPermission(
            result,
            Intent.FLAG_GRANT_READ_URI_PERMISSION
        )

        val uriString = result.toString()
        val folder = gamesViewModel.folders.value.firstOrNull {
            it.uriString == uriString && it.type == DirectoryType.EXTERNAL_CONTENT
        }
        if (folder != null) {
            Toast.makeText(
                requireContext().applicationContext,
                R.string.folder_already_added,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        val externalContentDir = GameDir(uriString, deepScan = false, DirectoryType.EXTERNAL_CONTENT)
        gamesViewModel.addFolder(externalContentDir, savedFromGameFragment = false)
    }

    private fun setInsets() =
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.toolbarFolders.updateMargins(left = leftInsets, right = rightInsets)

            val fabSpacing = resources.getDimensionPixelSize(R.dimen.spacing_fab)
            binding.buttonAdd.updateMargins(
                left = leftInsets + fabSpacing,
                right = rightInsets + fabSpacing,
                bottom = barInsets.bottom + fabSpacing
            )

            binding.listFolders.updateMargins(left = leftInsets, right = rightInsets)

            binding.listFolders.updatePadding(
                bottom = barInsets.bottom +
                    resources.getDimensionPixelSize(R.dimen.spacing_bottom_list_fab)
            )

            windowInsets
        }
}
