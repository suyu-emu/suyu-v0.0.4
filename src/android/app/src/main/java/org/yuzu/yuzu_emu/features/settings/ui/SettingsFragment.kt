// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

package org.yuzu.yuzu_emu.features.settings.ui

import android.annotation.SuppressLint
import android.content.Intent
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings as AndroidSettings
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
import androidx.navigation.findNavController
import androidx.navigation.fragment.navArgs
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import com.google.android.material.transition.MaterialSharedAxis
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.databinding.FragmentSettingsBinding
import org.yuzu.yuzu_emu.features.input.NativeInput
import org.yuzu.yuzu_emu.features.settings.model.Settings
import org.yuzu.yuzu_emu.features.settings.model.view.PathSetting
import org.yuzu.yuzu_emu.fragments.MessageDialogFragment
import org.yuzu.yuzu_emu.utils.PathUtil
import org.yuzu.yuzu_emu.utils.ViewUtils.updateMargins
import org.yuzu.yuzu_emu.utils.*
import java.io.File
import androidx.core.net.toUri

class SettingsFragment : Fragment() {
    private lateinit var presenter: SettingsFragmentPresenter
    private var settingsAdapter: SettingsAdapter? = null

    private var _binding: FragmentSettingsBinding? = null
    private val binding get() = _binding!!

    private val args by navArgs<SettingsFragmentArgs>()

    private val settingsViewModel: SettingsViewModel by activityViewModels()

    private val requestAllFilesPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.StartActivityForResult()
    ) {
        if (hasAllFilesPermission()) {
            showPathPickerDialog()
        } else {
            Toast.makeText(
                requireContext(),
                R.string.all_files_permission_required,
                Toast.LENGTH_LONG
            ).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enterTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)
        returnTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        reenterTransition = MaterialSharedAxis(MaterialSharedAxis.X, false)
        exitTransition = MaterialSharedAxis(MaterialSharedAxis.X, true)

        val playerIndex = getPlayerIndex()
        if (playerIndex != -1) {
            NativeInput.loadInputProfiles()
            NativeInput.reloadInputDevices()
        }
    }

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentSettingsBinding.inflate(layoutInflater)
        return binding.root
    }

    @SuppressLint("NotifyDataSetChanged")
    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        super.onViewCreated(view, savedInstanceState)
        settingsAdapter = SettingsAdapter(this, requireContext())
        presenter = SettingsFragmentPresenter(
            settingsViewModel,
            settingsAdapter!!,
            args.menuTag,
            activity
        )

        binding.toolbarSettingsLayout.title = if (args.menuTag == Settings.MenuTag.SECTION_ROOT &&
            args.game != null
        ) {
            args.game!!.title
        } else {
            when (args.menuTag) {
                Settings.MenuTag.SECTION_INPUT_PLAYER_ONE -> Settings.getPlayerString(1)
                Settings.MenuTag.SECTION_INPUT_PLAYER_TWO -> Settings.getPlayerString(2)
                Settings.MenuTag.SECTION_INPUT_PLAYER_THREE -> Settings.getPlayerString(3)
                Settings.MenuTag.SECTION_INPUT_PLAYER_FOUR -> Settings.getPlayerString(4)
                Settings.MenuTag.SECTION_INPUT_PLAYER_FIVE -> Settings.getPlayerString(5)
                Settings.MenuTag.SECTION_INPUT_PLAYER_SIX -> Settings.getPlayerString(6)
                Settings.MenuTag.SECTION_INPUT_PLAYER_SEVEN -> Settings.getPlayerString(7)
                Settings.MenuTag.SECTION_INPUT_PLAYER_EIGHT -> Settings.getPlayerString(8)
                else -> getString(args.menuTag.titleId)
            }
        }

        binding.listSettings.apply {
            adapter = settingsAdapter
            layoutManager = LinearLayoutManager(requireContext())
        }

        binding.toolbarSettings.setNavigationOnClickListener {
            settingsViewModel.setShouldNavigateBack(true)
        }

        settingsViewModel.shouldReloadSettingsList.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setShouldReloadSettingsList(false) }
        ) { if (it) presenter.loadSettingsList() }
        settingsViewModel.adapterItemChanged.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setAdapterItemChanged(-1) }
        ) { if (it != -1) settingsAdapter?.notifyItemChanged(it) }
        settingsViewModel.datasetChanged.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setDatasetChanged(false) }
        ) { if (it) settingsAdapter?.notifyDataSetChanged() }
        settingsViewModel.reloadListAndNotifyDataset.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setReloadListAndNotifyDataset(false) }
        ) { if (it) presenter.loadSettingsList(true) }
        settingsViewModel.shouldShowResetInputDialog.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setShouldShowResetInputDialog(false) }
        ) {
            if (it) {
                MessageDialogFragment.newInstance(
                    activity = requireActivity(),
                    titleId = R.string.reset_mapping,
                    descriptionId = R.string.reset_mapping_description,
                    positiveAction = {
                        NativeInput.resetControllerMappings(getPlayerIndex())
                        settingsViewModel.setReloadListAndNotifyDataset(true)
                    },
                    negativeAction = {}
                ).show(parentFragmentManager, MessageDialogFragment.TAG)
            }
        }

        settingsViewModel.shouldShowPathPicker.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setShouldShowPathPicker(false) }
        ) {
            if (it) {
                handlePathPickerRequest()
            }
        }

        settingsViewModel.shouldShowPathResetDialog.collect(
            viewLifecycleOwner,
            resetState = { settingsViewModel.setShouldShowPathResetDialog(false) }
        ) {
            if (it) {
                showPathResetDialog()
            }
        }

        if (args.menuTag == Settings.MenuTag.SECTION_ROOT) {
            binding.toolbarSettings.inflateMenu(R.menu.menu_settings)
            binding.toolbarSettings.setOnMenuItemClickListener {
                when (it.itemId) {
                    R.id.action_search -> {
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

    private fun getPlayerIndex(): Int =
        when (args.menuTag) {
            Settings.MenuTag.SECTION_INPUT_PLAYER_ONE -> 0
            Settings.MenuTag.SECTION_INPUT_PLAYER_TWO -> 1
            Settings.MenuTag.SECTION_INPUT_PLAYER_THREE -> 2
            Settings.MenuTag.SECTION_INPUT_PLAYER_FOUR -> 3
            Settings.MenuTag.SECTION_INPUT_PLAYER_FIVE -> 4
            Settings.MenuTag.SECTION_INPUT_PLAYER_SIX -> 5
            Settings.MenuTag.SECTION_INPUT_PLAYER_SEVEN -> 6
            Settings.MenuTag.SECTION_INPUT_PLAYER_EIGHT -> 7
            else -> -1
        }

    private fun setInsets() {
        ViewCompat.setOnApplyWindowInsetsListener(
            binding.root
        ) { _: View, windowInsets: WindowInsetsCompat ->
            val barInsets = windowInsets.getInsets(WindowInsetsCompat.Type.systemBars())
            val cutoutInsets = windowInsets.getInsets(WindowInsetsCompat.Type.displayCutout())

            val leftInsets = barInsets.left + cutoutInsets.left
            val rightInsets = barInsets.right + cutoutInsets.right

            binding.listSettings.updateMargins(left = leftInsets, right = rightInsets)
            binding.listSettings.updatePadding(bottom = barInsets.bottom)

            binding.appbarSettings.updateMargins(left = leftInsets, right = rightInsets)
            windowInsets
        }
    }

    private fun hasAllFilesPermission(): Boolean {
        return if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            Environment.isExternalStorageManager()
        } else {
            true
        }
    }

    private fun requestAllFilesPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            val intent = Intent(AndroidSettings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
            intent.data = "package:${requireContext().packageName}".toUri()
            requestAllFilesPermissionLauncher.launch(intent)
        }
    }

    private fun handlePathPickerRequest() {
        if (!hasAllFilesPermission()) {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.all_files_permission_required)
                .setMessage(R.string.all_files_permission_required)
                .setPositiveButton(R.string.grant_permission) { _, _ ->
                    requestAllFilesPermission()
                }
                .setNegativeButton(R.string.cancel, null)
                .show()
            return
        }
        showPathPickerDialog()
    }

    private fun showPathPickerDialog() {
        directoryPickerLauncher.launch(null)
    }

    private val directoryPickerLauncher = registerForActivityResult(
        ActivityResultContracts.OpenDocumentTree()
    ) { uri ->
        if (uri != null) {
            val pathSetting = settingsViewModel.clickedItem as? PathSetting ?: return@registerForActivityResult
            val rawPath = PathUtil.getPathFromUri(uri)
            if (rawPath != null) {
                handleSelectedPath(pathSetting, rawPath)
            } else {
                Toast.makeText(
                    requireContext(),
                    R.string.invalid_directory,
                    Toast.LENGTH_SHORT
                ).show()
            }
        }
    }

    private fun handleSelectedPath(pathSetting: PathSetting, path: String) {
        if (!PathUtil.validateDirectory(path)) {
            Toast.makeText(
                requireContext(),
                R.string.invalid_directory,
                Toast.LENGTH_SHORT
            ).show()
            return
        }

        if (pathSetting.pathType == PathSetting.PathType.SAVE_DATA) {
            val oldPath = pathSetting.getCurrentPath()
            if (oldPath != path) {
                promptSaveMigration(pathSetting, oldPath, path)
            }
        } else {
            setPathAndNotify(pathSetting, path)
        }
    }

    private fun promptSaveMigration(pathSetting: PathSetting, fromPath: String, toPath: String) {
        val sourceSavePath = "$fromPath/user/save"
        val destSavePath = "$toPath/user/save"
        val sourceSaveDir = File(sourceSavePath)
        val destSaveDir = File(destSavePath)

        val sourceHasSaves = PathUtil.hasContent(sourceSavePath)
        val destHasSaves = PathUtil.hasContent(destSavePath)

        if (!sourceHasSaves) {
            setPathAndNotify(pathSetting, toPath)
            return
        }

        if (destHasSaves) {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.migrate_save_data)
                .setMessage(R.string.destination_has_saves)
                .setPositiveButton(R.string.confirm) { _, _ ->
                    migrateSaveData(pathSetting, sourceSaveDir, destSaveDir, toPath)
                }
                .setNegativeButton(R.string.skip_migration) { _, _ ->
                    setPathAndNotify(pathSetting, toPath)
                }
                .setNeutralButton(R.string.cancel, null)
                .show()
        } else {
            MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.migrate_save_data)
                .setMessage(R.string.migrate_save_data_question)
                .setPositiveButton(R.string.confirm) { _, _ ->
                    migrateSaveData(pathSetting, sourceSaveDir, destSaveDir, toPath)
                }
                .setNegativeButton(R.string.skip_migration) { _, _ ->
                    setPathAndNotify(pathSetting, toPath)
                }
                .setNeutralButton(R.string.cancel, null)
                .show()
        }
    }

    private fun migrateSaveData(
        pathSetting: PathSetting,
        sourceDir: File,
        destDir: File,
        newPath: String
    ) {
        Thread {
            val success = PathUtil.copyDirectory(sourceDir, destDir, overwrite = true)

            requireActivity().runOnUiThread {
                if (success) {
                    setPathAndNotify(pathSetting, newPath)
                    Toast.makeText(
                        requireContext(),
                        R.string.save_migration_complete,
                        Toast.LENGTH_SHORT
                    ).show()
                } else {
                    Toast.makeText(
                        requireContext(),
                        R.string.save_migration_failed,
                        Toast.LENGTH_SHORT
                    ).show()
                }
            }
        }.start()
    }

    private fun setPathAndNotify(pathSetting: PathSetting, path: String) {
        pathSetting.setPath(path)
        NativeConfig.saveGlobalConfig()

        NativeConfig.reloadGlobalConfig()

        val messageResId = if (pathSetting.pathType == PathSetting.PathType.SAVE_DATA) {
            R.string.save_directory_set
        } else {
            R.string.path_set
        }

        Toast.makeText(
            requireContext(),
            messageResId,
            Toast.LENGTH_SHORT
        ).show()

        val position = settingsViewModel.pathSettingPosition.value
        if (position >= 0) {
            settingsAdapter?.notifyItemChanged(position)
        }
    }

    private fun showPathResetDialog() {
        val pathSetting = settingsViewModel.clickedItem as? PathSetting ?: return

        if (pathSetting.isUsingDefaultPath()) {
            return
        }

        val currentPath = pathSetting.getCurrentPath()
        val defaultPath = pathSetting.getDefaultPath()

        MaterialAlertDialogBuilder(requireContext())
            .setTitle(R.string.reset_to_nand)
            .setMessage(R.string.migrate_save_data_question)
            .setPositiveButton(R.string.confirm) { _, _ ->
                val sourceSaveDir = File(currentPath, "user/save")
                val destSaveDir = File(defaultPath, "user/save")

                if (sourceSaveDir.exists() && sourceSaveDir.listFiles()?.isNotEmpty() == true) {
                    migrateSaveData(pathSetting, sourceSaveDir, destSaveDir, defaultPath)
                } else {
                    setPathAndNotify(pathSetting, defaultPath)
                }
            }
            .setNegativeButton(R.string.cancel) { _, _ ->
                // just dismiss
            }
            .show()
    }
}
