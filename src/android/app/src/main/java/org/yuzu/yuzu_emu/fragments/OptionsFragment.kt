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
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import androidx.fragment.app.Fragment
import androidx.fragment.app.activityViewModels
import androidx.lifecycle.lifecycleScope
import androidx.preference.PreferenceManager
import androidx.recyclerview.widget.LinearLayoutManager
import com.google.android.material.dialog.MaterialAlertDialogBuilder
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.yuzu.yuzu_emu.NativeLibrary
import org.yuzu.yuzu_emu.R
import org.yuzu.yuzu_emu.adapters.HomeOptionAdapter
import org.yuzu.yuzu_emu.databinding.DialogProgressBarBinding
import org.yuzu.yuzu_emu.databinding.FragmentOptionsBinding
import org.yuzu.yuzu_emu.features.settings.ui.SettingsActivity
import org.yuzu.yuzu_emu.features.settings.utils.SettingsFile
import org.yuzu.yuzu_emu.model.GamesViewModel
import org.yuzu.yuzu_emu.model.HomeOption
import org.yuzu.yuzu_emu.utils.DirectoryInitialization
import org.yuzu.yuzu_emu.utils.FileUtil
import org.yuzu.yuzu_emu.utils.GameHelper
import org.yuzu.yuzu_emu.utils.GpuDriverHelper
import java.io.IOException

class OptionsFragment : Fragment() {
    private var _binding: FragmentOptionsBinding? = null
    private val binding get() = _binding!!

    private val gamesViewModel: GamesViewModel by activityViewModels()

    override fun onCreateView(
        inflater: LayoutInflater,
        container: ViewGroup?,
        savedInstanceState: Bundle?
    ): View {
        _binding = FragmentOptionsBinding.inflate(layoutInflater)
        return binding.root
    }

    override fun onViewCreated(view: View, savedInstanceState: Bundle?) {
        val optionsList: List<HomeOption> = listOf(
            HomeOption(
                R.string.add_games,
                R.string.add_games_description,
                R.drawable.ic_add
            ) { getGamesDirectory.launch(Intent(Intent.ACTION_OPEN_DOCUMENT_TREE).data) },
            HomeOption(
                R.string.install_prod_keys,
                R.string.install_prod_keys_description,
                R.drawable.ic_unlock
            ) { getProdKey.launch(arrayOf("*/*")) },
            HomeOption(
                R.string.install_amiibo_keys,
                R.string.install_amiibo_keys_description,
                R.drawable.ic_nfc
            ) { getAmiiboKey.launch(arrayOf("*/*")) },
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

        requireActivity().window.statusBarColor = ThemeHelper.getColorWithOpacity(
            MaterialColors.getColor(
                binding.root,
                R.attr.colorSurface
            ), ThemeHelper.SYSTEM_BAR_ALPHA
        )

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
                getDriver.launch(arrayOf("application/zip"))
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

    private val getGamesDirectory =
        registerForActivityResult(ActivityResultContracts.OpenDocumentTree()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            requireActivity().contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            // When a new directory is picked, we currently will reset the existing games
            // database. This effectively means that only one game directory is supported.
            PreferenceManager.getDefaultSharedPreferences(requireContext()).edit()
                .putString(GameHelper.KEY_GAME_PATH, result.toString())
                .apply()

            gamesViewModel.reloadGames(true)
        }

    private val getProdKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            requireActivity().contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val dstPath = DirectoryInitialization.userDirectory + "/keys/"
            if (FileUtil.copyUriToInternalStorage(requireContext(), result, dstPath, "prod.keys")) {
                if (NativeLibrary.reloadKeys()) {
                    Toast.makeText(
                        requireContext(),
                        R.string.install_keys_success,
                        Toast.LENGTH_SHORT
                    ).show()
                    gamesViewModel.reloadGames(true)
                } else {
                    Toast.makeText(
                        requireContext(),
                        R.string.install_keys_failure,
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }

    private val getAmiiboKey =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            requireActivity().contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val dstPath = DirectoryInitialization.userDirectory + "/keys/"
            if (FileUtil.copyUriToInternalStorage(
                    requireContext(),
                    result,
                    dstPath,
                    "key_retail.bin"
                )
            ) {
                if (NativeLibrary.reloadKeys()) {
                    Toast.makeText(
                        requireContext(),
                        R.string.install_keys_success,
                        Toast.LENGTH_SHORT
                    ).show()
                } else {
                    Toast.makeText(
                        requireContext(),
                        R.string.install_amiibo_keys_failure,
                        Toast.LENGTH_LONG
                    ).show()
                }
            }
        }

    private val getDriver =
        registerForActivityResult(ActivityResultContracts.OpenDocument()) { result ->
            if (result == null)
                return@registerForActivityResult

            val takeFlags =
                Intent.FLAG_GRANT_WRITE_URI_PERMISSION or Intent.FLAG_GRANT_READ_URI_PERMISSION
            requireActivity().contentResolver.takePersistableUriPermission(
                result,
                takeFlags
            )

            val progressBinding = DialogProgressBarBinding.inflate(layoutInflater)
            progressBinding.progressBar.isIndeterminate = true
            val installationDialog = MaterialAlertDialogBuilder(requireContext())
                .setTitle(R.string.installing_driver)
                .setView(progressBinding.root)
                .show()

            lifecycleScope.launch {
                withContext(Dispatchers.IO) {
                    // Ignore file exceptions when a user selects an invalid zip
                    try {
                        GpuDriverHelper.installCustomDriver(requireContext(), result)
                    } catch (_: IOException) {
                    }

                    withContext(Dispatchers.Main) {
                        installationDialog.dismiss()

                        val driverName = GpuDriverHelper.customDriverName
                        if (driverName != null) {
                            Toast.makeText(
                                requireContext(),
                                getString(
                                    R.string.select_gpu_driver_install_success,
                                    driverName
                                ),
                                Toast.LENGTH_SHORT
                            ).show()
                        } else {
                            Toast.makeText(
                                requireContext(),
                                R.string.select_gpu_driver_error,
                                Toast.LENGTH_LONG
                            ).show()
                        }
                    }
                }
            }
        }
}
